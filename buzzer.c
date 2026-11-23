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
int buzzer_running = 0;


/**
 * Initializes the pin and timer for the buzzer. More specifically, the following are initialized:
 * D8 (PI2) -> Buzzer Pin
 * TIM2 -> timer for oscillation HZ
 * TIM4 -> timer for oscillation
 */
void buzzer_init(){
	RCC->AHB1ENR |= (1UL << 8);
	RCC->APB1ENR |= (1 << 0) | (1 << 2);
	// Initializes Pin D8 (PI2) to output low
	GPIOI->MODER &= ~(3UL << 2 * 2);
	GPIOI->MODER |= (1UL << 2 * 2);
	GPIOI->BSRR |= (1UL << (2 + 16));

	// Setting TIM2 to ring device at 400 HZ
	TIM2->PSC = 9999; // Divides 16 MHz clock by 20000 for a 800 Hz timer
	TIM2->ARR = 1; // Resets every 2 ticks
	TIM2->EGR |= (1UL << 0);        // latch PSC/ARR
	TIM2->SR &= ~(1UL << 0);
	TIM2->DIER |= 1UL;
	NVIC->ISER[0] |= (1UL << 28);
	NVIC->IP[28] = (5UL << 4);  // Sets the buzzer interrupt to a priority of 5
	// We are not enabling the clock yet because the buzzer does not need to run

	TIM4->PSC = 15999; // Makes a 1ms-long tick
	TIM4->ARR = 1000; // REset will be changed in the buzzer_run function
	TIM4->EGR |= (1UL << 0);        // latch PSC/ARR
	TIM4->SR &= ~(1UL << 0);
	TIM4->DIER |= 1UL;
	NVIC->ISER[0] |= (1UL << 30);
	NVIC->IP[30] = (3UL << (4)); // sets a higher priority of the TIM4 interrupt
	// Timer will not enabled until buzzer_run

}


void buzzer_run(uint32_t millis){
	if (millis < 2) millis = 2;
	buzzer_running = 1;
	TIM2->CR1 &= ~1UL;
	TIM4->CR1 &= ~1UL;
	TIM2->CNT = 0;
	TIM4->CNT = 0;
	TIM2->SR = 0;
	TIM4->SR = 0;
	NVIC->ICPR[0] = (1UL << 28) | (1UL << 30);
	TIM4->ARR = millis - 1;
	TIM2->CR1 |= 1UL;
	TIM4->CR1 |= 1UL;
	while(buzzer_running);
}

/**
 * Handle the interrupt for when the buzzer needs to be bit-flipped to enable square
 * wave oscillations to the buzzer
 */
void TIM2_IRQHandler(){
	TIM2->SR &= ~(1 << 0);
	GPIOI->BSRR |= (1 << (2 + (BUZZER_PIN_ON ? 16 : 0)));
	BUZZER_PIN_ON = !BUZZER_PIN_ON;
	TIM2->SR &= ~(1 << 0);
}

/**
 * Turns off buzzer oscillations and the timer.
 */
void TIM4_IRQHandler(){
	TIM2->SR &= ~(1 << 0);
	TIM4->SR &= ~(1 << 0);
	GPIOI->BSRR |= (1 << (2 + 16));
	TIM2->CR1 &= ~(1 << 0);
	TIM4->CR1 &= ~(1 << 0);
	TIM2->SR &= ~(1 << 0);
	TIM4->SR &= ~(1 << 0);
	buzzer_running = 0;
}
