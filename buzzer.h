/**
 * @author Robert Cromer
 * @file buzzer.h
 * @brief contains the prototypes for the buzzer functions
 */

#include <stdint.h>

#ifndef BUZZER_H
#define BUZZER_H

/**
 * Initializes the pin and timer for the buzzer. More specifically, the following are initialized:
 * D8 (PI2) -> Buzzer Pin
 * TIM2 -> timer for oscillation HZ
 * TIM4 -> timer for oscillation
 */
void buzzer_init();

/**
 * @brief Has the buzzer play a 400 Hz note
 * @param millis : the milliseconds the buzzer will run
 */
void buzzer_run(uint32_t);

#endif
