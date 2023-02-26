#include <gpio.h>
#include <stm32.h>
#include <string.h>
#include <irq.h>
#include "buffer.h"
#include "adc.h"
#include "delay.h"
#include "config.h"
#include "bluetooth.h"

// **************** USART ******************************

#define USART_Mode_Rx_Tx (USART_CR1_RE | USART_CR1_TE)
#define USART_Enable USART_CR1_UE

#define USART_WordLength_8b 0x0000
#define USART_WordLength_9b USART_CR1_M

#define USART_Parity_No 0x0000
#define USART_Parity_Even USART_CR1_PCE
#define USART_Parity_Odd (USART_CR1_PCE | USART_CR1_PS)

#define USART_StopBits_1 0x0000

#define USART_FlowControl_None 0x0000
#define USART_FlowControl_RTS USART_CR3_RTSE
#define USART_FlowControl_CTS USART_CR3_CTSE

#define USART_New_Input (USART1->SR & USART_SR_RXNE)

#define USART_Free_Output (USART1->SR & USART_SR_TXE)

// ***************** Misc ******************************

#define HSI_HZ 16000000U

#define PCLK1_HZ HSI_HZ

#define BAUD_RATE 38400U

// **************  last handled interrupt to avoid starvation *********

int bluetooth_usart_init()
{
    // enable gpio for bluetooth usart and dma
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN |
                    RCC_AHB1ENR_DMA2EN;

    // for ext. interrupts
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN | RCC_APB2ENR_USART1EN;

    __NOP();
    __NOP();

    // uart pins config
    // tx
    GPIOafConfigure(GPIOA,
                    9,
                    GPIO_OType_PP,
                    GPIO_Fast_Speed,
                    GPIO_PuPd_NOPULL,
                    GPIO_AF_USART1);

    // pull up to avoid random transfer at boot
    // rx
    GPIOafConfigure(GPIOA,
                    10,
                    GPIO_OType_PP,
                    GPIO_Fast_Speed,
                    GPIO_PuPd_UP,
                    GPIO_AF_USART1);

    USART1->CR1 = USART_Mode_Rx_Tx | USART_WordLength_8b | USART_Parity_No;

    USART1->CR2 = USART_StopBits_1;

    USART1->CR3 = USART_FlowControl_None;

    USART1->BRR = (PCLK1_HZ + (BAUD_RATE / 2U)) / BAUD_RATE;

    // enable dma for tx and rx

    USART1->CR3 = USART_CR3_DMAT | USART_CR3_DMAR;
    return 0;
}

int bluetooth_dma_init()
{

    // config DMA tx

    // select channel 4, medium priority, buffer increment, mem -> uart,
    //  interrupt on transfer end only
    DMA2_Stream7->CR = 4U << DMA_SxCR_CHSEL_Pos |
                       DMA_SxCR_PL_1 |
                       DMA_SxCR_MINC |
                       DMA_SxCR_DIR_0 |
                       DMA_SxCR_TCIE;

    // data register to write to
    DMA2_Stream7->PAR = (uint32_t)&USART1->DR;

    // config dma rx
    // select channel 4, medium priority, buffer increment, uart -> mem,
    // interrupt on transfer end only
    DMA2_Stream5->CR = 4U << DMA_SxCR_CHSEL_Pos |
                       DMA_SxCR_PL_1 |
                       DMA_SxCR_MINC |
                       DMA_SxCR_TCIE;

    // data register to recive from
    DMA2_Stream5->PAR = (uint32_t)&USART1->DR;

    // clear transfer complete flag from reset
    DMA2->HIFCR = DMA_HIFCR_CTCIF7 |
                  DMA_HIFCR_CTCIF5;

    NVIC_SetPriorityGrouping(3);

    uint32_t dma_priority = NVIC_EncodePriority(3, 14, 0);

    NVIC_SetPriority(DMA2_Stream7_IRQn, dma_priority);
    NVIC_SetPriority(DMA2_Stream5_IRQn, dma_priority);

    NVIC_EnableIRQ(DMA2_Stream7_IRQn);
    NVIC_EnableIRQ(DMA2_Stream5_IRQn);
    return dma_priority;
}

int bluetooth_usart_start()
{
    // starts listening here
    USART1->CR1 |= USART_Enable;
    return 0;
}

int bluetooth_dma_start(char* dma_in)
{
    // init recieve
    DMA2_Stream5->M0AR = (uint32_t)dma_in;
    DMA2_Stream5->NDTR = 1;
    DMA2_Stream5->CR |= DMA_SxCR_EN;
    return 0;
}

int bluetooth_run(char* dma_in)
{
    bluetooth_usart_init();
    uint32_t dma_priority = bluetooth_dma_init();
    bluetooth_usart_start();
    bluetooth_dma_start(dma_in);

    return dma_priority;
}