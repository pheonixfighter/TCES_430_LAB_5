/**
 * @author : Robert Cromer
 * @file : buzzer.c
 * @brief : Contains the code for the buzzer functions
 */

#include "buzzer.h"
#include <stdint.h>
#include "stm32f746xx.h"

int BUZZER_COUNTS = 0;
int BUZZER_PIN_ON = 0;



/**
 * Initializes the pin and timer for the buzzer. More specifically, the following are initialized:
 * D8 (PI2) -> Buzzer Pin
 * TIM2 -> timer for oscillation HZ
 * TIM4 -> timer for oscillation
 */
void buzzer_init(){
	// Initializes Pin D8 (PI2) to output low
	GPIOI->MODER &= ~(3UL << 2 * 2);
	GPIOI->MODER |= (1UL << 2 * 2);
	GPIOI->BSRR |= (1UL << (2 + 16));

	// Setting TIM2 to ring device at 400 HZ
	TIM2->PSC = 19999; // Divides 16 MHz clock by 20000 for a 800 Hz timer
	TIM2->ARR = 400; // Resets every 400 ticks
	TIM2->SR &= ~(1UL << 0);
	NVIC->ISER[0] |= (1UL << 28);
	NVIC->IP[7] &= ~(0xFUL << 0);
	NVIC->IP[7] |= 5; // Sets the buzzer interrupt to a priority of 5
	// We are not enabling the clock yet because the buzzer does not need to run

	TIM4->PSC = 15999; // Makes a 1ms-long tick
	TIM4->ARR = 1000; // REset will be changed in the buzzer_run function
	TIM4->SR &= ~(1UL << 0);
	NVIC->ISER[0] |= (1UL << 30);
	NVIC->IP[7] &= (0xFUL << (4 * 2));
	NVIC->IP[7] |= (5UL << (4 * 2));
	// Timer will not enabled until buzzer_run

}


void buzzer_run(int millis){
	__disable_irq();
	TIM4->ARR = millis;
	__enable_irq();

	TIM2->CR1 |= (1UL << 0);
	TIM4->CR1 |= (1UL << 0);
}

/**
 * Handle the interrupt for when the buzzer needs to be bit-flipped to enable square
 * wave oscillations to the buzzer
 */
void TIM2_IRQHandler(){
	GPIOI->BSRR |= (1 << (2 + (BUZZER_PIN_ON ? 16 : 0)));
	BUZZER_PIN_ON = !BUZZER_PIN_ON;
	TIM2->SR &= ~(1 << 0);
}

/**
 * Turns off buzzer oscillations and the timer.
 */
void TIM4_IRQHandler(){
	GPIOI->BSRR |= (1 << (2 + 16));
	TIM2->CR1 &= ~(1 << 0);
	TIM4->CR1 &= ~(1 << 0);
	TIM2->SR &= ~(1 << 0);
	TIM4->SR &= ~(1 << 0);
}
