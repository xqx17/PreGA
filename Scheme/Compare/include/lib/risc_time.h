#ifndef RISCV_CLOCK_H
#define RISCV_CLOCK_H

#include <stdint.h>

void systick_Init(void);
uint32_t Get_counter(void);

#endif // RISCV_CLOCK_H
