#include <gpio.h>
#include <stm32.h>
#include <string.h>
#include <irq.h>

#include "adc.h"
#include "buffer.h"
#include "delay.h"

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

#define USART_Free_Output (USART2->SR & USART_SR_TXE)

// ***************** Misc ******************************

#define HSI_HZ 16000000U

#define PCLK1_HZ HSI_HZ

#define BAUD_RATE 9600U

#define IN_MSG_LEN 3

#define OUT_MSG_MAX_LEN 4

// **************  last handled interrupt to avoid starvation *********

char dma_out[OUT_MSG_MAX_LEN] = {};

buffer out_buff;

uint16_t temp_val;

// ***************** Code *********************************

// after send
void DMA1_Stream6_IRQHandler()
{
    /* Odczytaj zgłoszone przerwania DMA1. */
    uint32_t isr = DMA1->HISR;
    if (isr & DMA_HISR_TCIF6)
    {
        memset(dma_out, '\0', OUT_MSG_MAX_LEN);
        /* Obsłuż zakończenie transferu
        w strumieniu 6. */
        DMA1->HIFCR = DMA_HIFCR_CTCIF6;
    }
}

int main()
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN |
                    RCC_AHB1ENR_GPIOBEN |
                    RCC_AHB1ENR_GPIOCEN |
                    RCC_AHB1ENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // for ext. interrupts
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    __NOP();
    __NOP();
    // pull up to avoid random transfer at boot
    GPIOafConfigure(GPIOA,
                    2,
                    GPIO_OType_PP,
                    GPIO_Fast_Speed,
                    GPIO_PuPd_NOPULL,
                    GPIO_AF_USART2);

    USART2->CR1 = USART_CR1_TE | USART_WordLength_8b | USART_Parity_No;

    USART2->CR2 = USART_StopBits_1;

    USART2->CR3 = USART_FlowControl_None;

    USART2->BRR = (PCLK1_HZ + (BAUD_RATE / 2U)) / BAUD_RATE;

    // enable dma

    USART2->CR3 = USART_CR3_DMAT;

    // config DMA tx

    // 4U becvasue channel 4, why << 25? becaues of chsel register bits 25-27
    DMA1_Stream6->CR = 4U << 25 |
                       DMA_SxCR_PL_1 |
                       DMA_SxCR_MINC |
                       DMA_SxCR_DIR_0 |
                       DMA_SxCR_TCIE;

    // data register to write to
    DMA1_Stream6->PAR = (uint32_t)&USART2->DR;

    DMA1->HIFCR = DMA_HIFCR_CTCIF6;

    NVIC_SetPriorityGrouping(3);

    uint32_t dma_priority = NVIC_EncodePriority(3, 14, 0);

    NVIC_SetPriority(DMA1_Stream6_IRQn, dma_priority);

    NVIC_EnableIRQ(DMA1_Stream6_IRQn);

    // starts listening here
    USART2->CR1 |= USART_Enable;
    adc_run(&temp_val);

    while (1)
    {
        memset(dma_out, '\0', OUT_MSG_MAX_LEN);
        uint16_t send_val = adc_calculate_temp(temp_val);
        dma_out[1] = (send_val % 10) + '0';
        send_val /= 10;
        dma_out[0] = (send_val % 10) + '0';
        dma_out[2] = '\n';
        //dma_out[1] = temp_val;
        //temp_val = temp_val >> 8;
        //dma_out[0] = temp_val;
        //dma_out[2] = '\n';

        if ((DMA1_Stream6->CR & DMA_SxCR_EN) == 0 &&
            (DMA1->HISR & DMA_HISR_TCIF6) == 0)
        {
            DMA1_Stream6->M0AR = (uint32_t)dma_out;
            DMA1_Stream6->NDTR = strlen(dma_out);
            DMA1_Stream6->CR |= DMA_SxCR_EN;
        }
        Delay(1000000);
    }

    return 1;
}