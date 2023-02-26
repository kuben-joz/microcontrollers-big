#include <gpio.h>
#include <stm32.h>
#include <string.h>
#include <irq.h>
#include "buffer.h"
#include "adc.h"
#include "delay.h"
#include "config.h"
#include "bluetooth.h"
#include "calibration.h"

// ***************** Misc ******************************

#define HSI_HZ 16000000U

#define PCLK1_HZ HSI_HZ

#define BAUD_RATE 38400U

int in_size = 0;
buffer out_buff;

char dma_out[OUT_MSG_LEN] = {};

char dma_in;

uint16_t measurements[RESAMPLING * 2];

int send_data = 0;

int needs_adc_start = 0;

// todo back to float
float tare = 0;

int needs_tare = 0;

// todo back to float
float average_weight = 0;

int weight_send = 0;

int adc_send = 0;

float coeff[3];

uint32_t dma_priority;

// ***************** Code *********************************

int timer_init()
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    TIM3->CR1 |= TIM_CR1_URS;  // only update on overflow, we are counting up
    TIM3->CR1 |= TIM_CR1_UDIS; // no updates for now
    TIM3->CR1 &= ~TIM_CR1_DIR; // counting up
    TIM3->PSC = 16000UL - 1;   // 1000hz
    TIM3->ARR = 50UL;          // 20 updates a second
    TIM3->CNT = 0UL;           // start at 0
    TIM3->EGR |= TIM_EGR_UG;   // init everything

    // enable interrupts
    TIM3->CCR1 = 51UL;          // enable interrupt on overflow
    TIM3->SR &= ~TIM_SR_UIF;    // zero out flags initially
    TIM3->DIER |= TIM_DIER_UIE; // set interrupt only on cc1

    uint32_t tim_priority = NVIC_EncodePriority(3, 15, 0); // allow this to be preemepted by DMA
    NVIC_SetPriority(TIM3_IRQn, tim_priority);
    NVIC_EnableIRQ(TIM3_IRQn);

    TIM3->CR1 |= TIM_CR1_CEN; // enable counter
    return 0;
}

int timer_enable()
{
    TIM3->CR1 &= ~TIM_CR1_UDIS;
    return 0;
}

int timer_disable()
{
    TIM3->CR1 |= TIM_CR1_UDIS;
    return 0;
}

void startMeasurements()
{
    if (!send_data)
    {
        send_data = 1;
        // adc_restart(RESAMPLING * 2, (uint32_t)measurements);
        needs_adc_start = 1;
        timer_enable();
    }
}

void stopMeasurements()
{
    if (send_data)
    {
        send_data = 0;
        timer_disable();
        adc_stop();
    }
}

void startCalibration()
{
    if (!send_data)
    {
        needs_adc_start = 1;
        send_data = 1;
    }
}

void endCalibration()
{
    if (send_data)
    {
        send_data = 0;
        adc_stop();
    }
}

void setNewMessage(int button_code, int new_state_code, char *msg)
{
}

void handleOutput(uint32_t but_changed, uint32_t new_state)
{
}

void handleInput(char command)
{
    switch ((int8_t)command)
    {
    case StartMeasurementsCode:
        startMeasurements();
        break;
    case StopMeasurementsCode:
        stopMeasurements();
        break;
    case StartCalibrationCode:
        startCalibration();
        break;
    case EndCalibrationCode:
        endCalibration();
        break;
    case TareCode:
        needs_tare = 1;
        break;
    case RequestADCValCode:
        adc_send = 1;
    }
}

// after send
void DMA2_Stream7_IRQHandler()
{
    // check if transfer done
    uint32_t isr = DMA2->HISR;
    if (isr & DMA_HISR_TCIF7)
    {
        DMA2->HIFCR = DMA_HIFCR_CTCIF7;
        if (weight_send)
        {
            weight_send = 0;
            average_weight -= tare;
            char *weight_val = (char *)&average_weight;
            dma_out[0] = WeightValCode;
            for (int i = 1; i < 5; i++)
            {
                dma_out[5 - i] = weight_val[i - 1];
            }
            dma_out[5] = 0;
            DMA2_Stream7->M0AR = (uint32_t)dma_out;
            DMA2_Stream7->NDTR = OUT_MSG_LEN;
            DMA2_Stream7->CR |= DMA_SxCR_EN;
        }
        else if (adc_send)
        {
            adc_send = 0;
        }
    }
}

// after recieve
void DMA2_Stream5_IRQHandler()
{
    // check if transfer done
    uint32_t isr = DMA2->HISR;
    if (isr & DMA_HISR_TCIF5)
    {
        /* Obsłuż zakończenie transferu
        w strumieniu 5. */
        DMA2->HIFCR = DMA_HIFCR_CTCIF5;
        handleInput(dma_in);
        /* Ponownie uaktywnij odbieranie. */
        DMA2_Stream5->M0AR = (uint32_t)&dma_in;
        DMA2_Stream5->NDTR = 1;
        DMA2_Stream5->CR |= DMA_SxCR_EN;
    }
}

void TIM3_IRQHandler(void)
{
    uint32_t it_status = TIM3->SR & TIM3->DIER;
    if (it_status & TIM_SR_UIF)
    {
        TIM3->SR &= ~TIM_SR_UIF;
        irq_level_t prot_level = IRQprotect(dma_priority);
        if ((DMA2_Stream7->CR & DMA_SxCR_EN) == 0 &&
            (DMA2->HISR & DMA_HISR_TCIF7) == 0)
        {
            IRQunprotect(prot_level);
            uint16_t adc_val_temp = 0;
            uint16_t adc_val_weight = 0;
            for (int i = 0; i < RESAMPLING; i++)
            {
                adc_val_temp += measurements[2 * i];
                adc_val_weight += measurements[2 * i + 1];
            }
            adc_val_temp /= RESAMPLING;
            adc_val_weight /= RESAMPLING;
            // todo change back
            // float average_temp = adc_calculate_temp(adc_val_temp);
            float average_temp = coeff[0];
            average_temp += coeff[1] * adc_val_temp;
            adc_val_temp = adc_val_temp * adc_val_temp;
            average_temp += coeff[2] * adc_val_temp;
            average_weight = adc_val_weight;
            if (needs_tare)
            {
                tare = average_weight;
                needs_tare = 0;
            }
            // todo change tare to be on weight not temp
            weight_send = 1;
            char *temp_val = (char *)&average_temp;
            dma_out[0] = TempValCode;
            for (int i = 1; i < 5; i++)
            {
                dma_out[5 - i] = temp_val[i - 1];
            }
            dma_out[5] = 0;
            DMA2_Stream7->M0AR = (uint32_t)dma_out;
            DMA2_Stream7->NDTR = OUT_MSG_LEN;
            DMA2_Stream7->CR |= DMA_SxCR_EN;
        }
        else
        {
            IRQunprotect(prot_level);
        }
    }
}

int main()
{
    NVIC_SetPriorityGrouping(3);
    timer_init();
    calibration_init();
    calibration_retrieve_polynomial(coeff);
    adc_run(RESAMPLING * 2, (uint32_t)measurements);
    dma_priority = bluetooth_run(&dma_in);

    while (1)
    {
        if (needs_adc_start)
        {
            // this is blocking so we are doing it in the main thread
            adc_restart(RESAMPLING * 2, (uint32_t)measurements);
            needs_adc_start = 0;
        }
        irq_level_t prot_level = IRQprotect(dma_priority);
        if ((adc_send &&
             DMA2_Stream7->CR & DMA_SxCR_EN) == 0 &&
            (DMA2->HISR & DMA_HISR_TCIF7) == 0)
        {
            adc_send = 0;
            IRQunprotect(prot_level);
            uint16_t adc_val_weight = 0;
            for (int i = 0; i < RESAMPLING; i++)
            {
                adc_val_weight += measurements[2 * i + 1];
            }
            adc_val_weight /= RESAMPLING;
            // todo change back
            // float average_temp = adc_calculate_temp(adc_val_temp);
            average_weight = adc_val_weight;
            // todo change tare to be on weight not temp
            char *temp_val = (char *)&average_weight;
            dma_out[0] = ADCValCode;
            for (int i = 1; i < 5; i++)
            {
                dma_out[5 - i] = temp_val[i - 1];
            }
            dma_out[5] = 0;
            DMA2_Stream7->M0AR = (uint32_t)dma_out;
            DMA2_Stream7->NDTR = OUT_MSG_LEN;
            DMA2_Stream7->CR |= DMA_SxCR_EN;
        }
        else
        {
            IRQunprotect(prot_level);
        }
        //__WFI();
    }

    return 1;
}