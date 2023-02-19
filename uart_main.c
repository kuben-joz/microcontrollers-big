#include <gpio.h>
#include <stm32.h>
#include <string.h>
#include <irq.h>

#include "adc.h"
#include "buffer.h"
#include "delay.h"

//****************** status led ************************
#define GreenLEDon() \
    GREEN_LED_GPIO->BSRR = 1 << (GREEN_LED_PIN + 16)
#define GreenLEDoff() \
    GREEN_LED_GPIO->BSRR = 1 << GREEN_LED_PIN
#define GreenLEDflip() \
    GREEN_LED_GPIO->BSRR = 1 << (GREEN_LED_PIN + 16 * (1 & (GREEN_LED_GPIO->ODR >> GREEN_LED_PIN)))
#define GREEN_LED_GPIO GPIOA
#define GREEN_LED_PIN 7

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

buffer out_buff;

uint16_t temp_val;

// ***************** Code *********************************

int main()
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    __NOP();
    __NOP();
    GreenLEDoff();
    GPIOoutConfigure(GREEN_LED_GPIO,
                     GREEN_LED_PIN,
                     GPIO_OType_PP,
                     GPIO_Low_Speed,
                     GPIO_PuPd_NOPULL);
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

    USART2->CR1 |= USART_Enable;

    adc_run(&temp_val);

    //int j = 0;
    int i = 3;
    char out[3];
    memset(out, '\0', 3);
    uint16_t send_val;
    int val_ready = 0;
    while (1)
    {

        if ((USART2->SR & USART_SR_TXE))
        {
            if (i == 3 && val_ready)
            {
                send_val = adc_calculate_temp(temp_val);
                // send_val = temp_val;
                out[1] = (send_val % 10) + '0';
                send_val /= 10;
                out[0] = (send_val % 10) + '0';
                out[2] = '\n';
                Delay(1000000);
                i = 0;
                val_ready = 0;
            }
            else if(i < 3)
            {
                USART2->DR = out[i++];
            }
        }
        else
        {
            Delay(1000000);
        }
        if((ADC1->SR & ADC_SR_EOC) && i==3) {
            temp_val = ADC1->DR;
            val_ready = 1;
            ADC1->CR2 |= ADC_CR2_SWSTART;
        }
        if((ADC1->SR & ADC_SR_OVR)) {
            ADC1->SR &= ~ADC_SR_OVR;
            ADC1->CR2 |= ADC_CR2_SWSTART;
            GreenLEDflip();
        }
    }

    // todo handle requests with RXNEIE
    return 1;
}