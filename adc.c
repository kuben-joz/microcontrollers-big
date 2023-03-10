#include <gpio.h>
#include <stm32.h>
#include <string.h>
#include <irq.h>
#include <delay.h>
#include "adc.h"

/*The ADC is powered on by setting the ADON bit in the ADC_CR2 register.

Conversion starts when either the SWSTART or the JSWSTART bit is set.
You can stop conversion and put the ADC in power down mode by clearing the ADON bit. In
this mode the ADC consumes almost no power (only a few μA).
*/

int adc_init(void)
{
    // enable gpio for measurment and DMA for comms with adc
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2EN;
    // enable ADC clock
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    // allow the peripheral clock enable delay to occur
    // 2 nops for ahb and 1 + (ahb/apb prescaler) for apb
    // todo check if I need more for apb if I slow it down
    // http://www.st.com/web/en/resource/technical/document/errata_sheet/DM00037591.pdf
    // 2.1.13
    __NOP();
    __NOP();

    GPIOainConfigure(GPIOA, 4);
    GPIOainConfigure(GPIOA, 5);

    // reference 11.12 in rm0383 for mappings below

    // todo adjust this to be in optimal range
    ADC1_COMMON->CCR |= 2UL << ADC_CCR_ADCPRE_Pos; // set prescaler to 6

    ADC1->CR1 &= ~ADC_CR1_RES; // set resolution to 12 bits
    ADC1->CR1 |= ADC_CR1_OVRIE;
    ADC1->CR1 |= ADC_CR1_SCAN; // enable scan mode

    ADC1->CR2 |= ADC_CR2_CONT;   // continuous conversion
    ADC1->CR2 |= ADC_CR2_DMA;    // enable DMA mode
    ADC1->CR2 |= ADC_CR2_DDS;    // keep issuing dma requests
    ADC1->CR2 &= ~ADC_CR2_ALIGN; // right alingmnet

    ADC1->SQR1 |= 1UL << ADC_SQR1_L_Pos; // set number of measurments to 2

    // todo use other pins if it lowers interference
    ADC1->SQR3 |= 4UL | 5UL << ADC_SQR3_SQ2_Pos; // first measurment on pin 4, second on 5

    // todo change this if charge up time is too slow for cap
    // use 3 cycles to measure each of our chosen channels
    ADC1->SMPR2 |= 4UL << ADC_SMPR2_SMP4_Pos;

    return 0;
}

int adc_enable(void)
{
    ADC1->CR2 |= ADC_CR2_ADON; // enable adc
    // todo not sure if it's these or those above that are neccessary
    //  I've seen this in example code
    Delay(1000);
    return 0;
}

int adc_start(void)
{

    // start the conversion
    // ADC1->SR = 0;
    // todo chekc if this trigger is req.
    // ADC1->CR2 |= (1UL << ADC_CR2_EXTEN_Pos); // rising edge trigger detect
    // todo add timer config EXTSEL
    ADC1->CR2 |= ADC_CR2_SWSTART; // regular channel start covnersion
    return 0;
}

int adc_dma_init(uint16_t transfer_num, uint32_t dest)
{
// now we configure the dma

// todo add else
#ifdef DMA2_STREAM0_CONFLICT
#error "DMA2 Stream 0 used for 2 different things"
#define DMA2_STREAM0_CONFLICT
#endif

    // see section 9.5 for register configs
    DMA2_Stream0->CR &= ~DMA_SxCR_CHSEL; // choose channel 0 for adc
    DMA2_Stream0->CR &= ~DMA_SxCR_DIR;   // peripheral -> memory
    DMA2_Stream0->CR |= DMA_SxCR_CIRC;   // circular mode
    // DMA2_Stream0->CR |= DMA_SxCR_MINC;  // auto memory increment
    //  todo change once strai sensor is added
    DMA2_Stream0->CR |= DMA_SxCR_MINC;             // memory address increment when we have more than one device
    DMA2_Stream0->CR &= ~DMA_SxCR_PINC;            // always use same register in adc
    DMA2_Stream0->CR |= 1UL << DMA_SxCR_PSIZE_Pos; // 16 bits on both sides
    DMA2_Stream0->CR |= 1UL << DMA_SxCR_MSIZE_Pos; // 16 bits on both sides

    DMA2_Stream0->NDTR = transfer_num;      // num of trasnfers of transfer size above
    DMA2_Stream0->PAR = (uint32_t)&ADC1->DR; // peripheral addr.
    DMA2_Stream0->M0AR = dest;               // set device distanation address
    // todo add M1AR if switching to double buffer mode
    NVIC_SetPriorityGrouping(3);
    uint32_t adc_priority = NVIC_EncodePriority(3, 13, 0);
    NVIC_SetPriority(ADC_IRQn, adc_priority);
    NVIC_EnableIRQ(ADC_IRQn);
    return 0;
}

int adc_dma_start(void)
{
    DMA2_Stream0->CR |= DMA_SxCR_EN;
    return 0;
}

static uint16_t adc_trans;
static uint32_t adc_addr;

int adc_run(uint16_t transfer_num, uint32_t dest)
{
    adc_trans = transfer_num;
    adc_addr = dest;
    adc_init();
    adc_dma_init(transfer_num, dest);

    return 0;
}

float adc_calculate_temp(uint16_t val)
{
    float res = 159.649273 - 0.0689190815 * val;
    return res;
}

float adc_calculate_weight(uint16_t val)
{
    float res = 159.649273 - 0.0689190815 * val;
    return res;
}

int adc_stop()
{
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    ADC1->CR2 &= ~ADC_CR2_ADON;
    return 0;
}

int adc_restart(uint16_t transfer_num, uint32_t dest)
{
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    ADC1->SR &= ~ADC_SR_OVR;
    adc_enable();
    DMA2_Stream0->NDTR = transfer_num;
    DMA2_Stream0->M0AR = dest;
    adc_dma_start();
    adc_start();
    return 0;
}

void ADC_IRQHandler(void)
{
    adc_restart(adc_trans, adc_addr);
}
