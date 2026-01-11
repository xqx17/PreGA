#include "risc_time.h"
#include "debug.h"

volatile uint32_t counter;

void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void SYSTICK_Init_Config(u_int64_t ticks)
{
    SysTick->SR &= ~(1 << 0);//clear State flag
    SysTick->CMP = ticks;
    SysTick->CNT = 0;
    SysTick->CTLR = 0xF;

    NVIC_SetPriority(SysTicK_IRQn, 15);
    NVIC_EnableIRQ(SysTicK_IRQn);
}

void systick_Init(void){
    counter = 0;
    SYSTICK_Init_Config(SystemCoreClock / 1000000 - 1);
}

uint32_t Get_counter(void){
    //printf("Counter value :%u\n",Get_counter());
    return counter;
}

void SysTick_Handler(void)
{
    if(SysTick->SR == 1)
    {
        SysTick->SR = 0;//clear State flag
        counter++;
    }
}
