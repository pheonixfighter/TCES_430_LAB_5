/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Robert Cromer
 * @brief          : Lab 5 - Interrupt-driven digital countdown safe lock
 *                   Keypad : EXTI interrupts fill keypad_character_buffer
 *                   LCD    : 4-bit bus, TIM3 polled delays
 *                   Buzzer : TIM2 (tone) + TIM4 (duration) interrupts
 *                   1 s timebase : TIM6 UIF flag, polled in the main loop
 ******************************************************************************
 */

#include <stdint.h>
#include "stm32f7xx.h"
#include "LCD.h"
#include "keypad.h"
#include "buzzer.h"
#include <string.h>

#if !defined(__SOFT_FP__) && defined(__ARM_FP)
  #warning "FPU is not initialized, but the project is compiling for an FPU. Please initialize the FPU before use."
#endif

#define SECRET_PIN          "1234"
#define PIN_LENGTH          4
#define MAX_ATTEMPTS        3
#define LOCKOUT_SECONDS     30
#define GRANTED_SECONDS     3       /* how long "Access Granted!" stays up */
#define KEY_BEEP_MS         100
#define GRANTED_TONE_MS     2000
#define LOCK_PULSE_MS       800

#define LCD_LINE_CHARS      16

typedef enum {
	STATE_ENTER_PIN,
	STATE_GRANTED,
	STATE_LOCKED
} States_t;

/* ----------------------- keypad buffer access ------------------------ */

/**
 * Pops the oldest key from the keypad ISR's buffer (FIFO).
 * Interrupts are masked during the pop so the ISR cannot add a key halfway
 * through the shift. Returns 0 if no key is waiting.
 */
static char keypad_pop(void){
	char key = 0;

	__disable_irq();
	if (keypad_character_buffer_index > 0){
		key = keypad_character_buffer[0];
		for (int i = 1; i < keypad_character_buffer_index; i++){
			keypad_character_buffer[i - 1] = keypad_character_buffer[i];
		}
		keypad_character_buffer_index--;
	}
	__enable_irq();

	return key;
}

/** Throws away every waiting key */
static void keypad_flush(void){
	__disable_irq();
	keypad_character_buffer_index = 0;
	__enable_irq();
}

/* ---------------------------- LCD printing --------------------------- */

/**
 * Copies a string into dest (no null terminator) and returns its length.
 */
static uint8_t copyText(char *dest, const char *src){
	uint8_t length = 0;
	while (*(src + length)){
		*(dest + length) = *(src + length);
		length++;
	}
	return length;
}

/**
 * Builds the text for the current state, then writes it to the LCD.
 *
 * @param state     the state whose screen is shown
 * @param value     STATE_ENTER_PIN: number of digits entered so far (shown as '*')
 *                  STATE_LOCKED   : seconds remaining (0 - 99)
 *                  STATE_GRANTED  : unused
 * @param wrongPin  STATE_ENTER_PIN only: nonzero shows the wrong-PIN message
 *                  on line 2 instead of the asterisks
 */
static void printStateToLCD(States_t state, uint8_t value, uint8_t wrongPin){
	char line1[LCD_LINE_CHARS + 1] = "";
	char line2[LCD_LINE_CHARS + 1] = "";

	switch (state){
	case STATE_ENTER_PIN:
		strcpy(line1, "Enter PIN:");
		if (wrongPin){
			strcpy(line2, "Wrong! Try again");
		} else {
			for (uint8_t i = 0; i < value && i < LCD_LINE_CHARS; i++){
				strcat(line2, "*");              /* one asterisk per digit */
			}
		}
		break;

	case STATE_GRANTED:
		strcpy(line1, "Access Granted!");
		break;

	case STATE_LOCKED: {
		char digits[3];
		digits[0] = '0' + (value / 10);
		digits[1] = '0' + (value % 10);
		digits[2] = '\0';

		strcpy(line1, "LOCKED!");
		strcpy(line2, "Time: ");
		strcat(line2, digits);                   /* "Time: " + "NN" */
		break;
	}
	}

	__disable_irq();
	LCDInit();
	sendCommand(0x01);
	TIM3_DelayMs(9);
	sendCommand(0x80);
	sendString(line1);
	sendCommand(0xC0);
	sendString(line2);
	__enable_irq();
}


/* ------------------- TIM6 polled 1 second timebase ------------------- */

static void timer6_init(void){
	TIM6->PSC = 15999;          /* 16 MHz / 16000 = 1 kHz tick             */
	TIM6->ARR = 999;            /* 1000 ticks (0..999) = exactly 1 second  */
	TIM6->EGR |= (1UL << 0);    /* latch PSC/ARR                           */
	TIM6->SR &= ~(1UL << 0);    /* clear the flag the update just set      */
}

static void timer6_start(void){
	TIM6->CR1 &= ~(1UL << 0);
	TIM6->CNT = 0;
	TIM6->SR &= ~(1UL << 0);
	TIM6->CR1 |= (1UL << 0);
}

static void timer6_stop(void){
	TIM6->CR1 &= ~(1UL << 0);
	TIM6->SR &= ~(1UL << 0);
}

/** Returns 1 once per second: flag is polled, then cleared manually */
static uint8_t timer6_tick(void){
	if (TIM6->SR & (1UL << 0)){
		TIM6->SR &= ~(1UL << 0);
		return 1;
	}
	return 0;
}

/* ------------------------------- main -------------------------------- */

int main(void)
{
	__disable_irq();
	States_t state = STATE_ENTER_PIN;
	char     entry[PIN_LENGTH];
	uint8_t  idx = 0;
	uint8_t  attempts = 0;
	uint8_t  seconds = 0;

	/* TIM6 clock (bit 4); TIM2/3/4 and the GPIO clocks are enabled by the
	   buzzer, LCD and keypad init functions. */
	RCC->APB1ENR |= (1UL << 4);

	LCDInit();              /* LCD pins + TIM3 delay timer + HD44780 setup */
	buzzer_init();
	timer6_init();
	keypad_initialize();     /* last: starts the EXTI interrupts            */
	printStateToLCD(STATE_ENTER_PIN, 0, 0);
	__enable_irq();


	while (1){
		char key = keypad_pop();      /* 0 when no key is waiting */

		switch (state){

		/* ---------------------- PIN entry ---------------------- */
		case STATE_ENTER_PIN:
			if (key){
				// the buzzer is started AFTER any LCD write, because the
				// write runs with interrupts disabled and would stall the tone
				uint32_t beepMs = KEY_BEEP_MS;

				entry[idx++] = key;
				printStateToLCD(STATE_ENTER_PIN, idx, 0);

				if (idx == PIN_LENGTH){
					uint8_t match = 1;
					for (uint8_t i = 0; i < PIN_LENGTH; i++){
						if (entry[i] != SECRET_PIN[i]) match = 0;
					}
					idx = 0;

					if (match){
						attempts = 0;
						printStateToLCD(STATE_GRANTED, 0, 0);
						beepMs = GRANTED_TONE_MS;
						seconds = GRANTED_SECONDS;
						timer6_start();
						state = STATE_GRANTED;
					} else {
						attempts++;
						if (attempts >= MAX_ATTEMPTS){
							seconds = LOCKOUT_SECONDS;
							printStateToLCD(STATE_LOCKED, seconds, 0);
							timer6_start();            /* start after drawing */
							state = STATE_LOCKED;
						} else {
							printStateToLCD(STATE_ENTER_PIN, 0, 1);
						}
					}
				}

				buzzer_run(beepMs);
				keypad_flush();    /* keys that arrived during the LCD write */
			}
			break;

		/* ------------------ Access granted --------------------- */
		case STATE_GRANTED:
			/* Keys are popped above and ignored. The 2 s tone is produced by
			   the buzzer interrupts, so nothing else to do here. */
			if (timer6_tick()){
				if (--seconds == 0){
					timer6_stop();
					keypad_flush();
					printStateToLCD(STATE_ENTER_PIN, 0, 0);
					state = STATE_ENTER_PIN;
				}
			}
			break;

		/* ---------------- Lockout countdown -------------------- */
		case STATE_LOCKED:
			/* Keys are popped above and discarded -> input is locked out.
			   The loop never blocks, so the countdown keeps running. */
			if (timer6_tick()){
				seconds--;
				buzzer_run(500);
				if (seconds == 0){
					timer6_stop();
					attempts = 0;
					keypad_flush();
					printStateToLCD(STATE_ENTER_PIN, 0, 0);
					state = STATE_ENTER_PIN;
				} else {
					printStateToLCD(STATE_LOCKED, seconds, 0);
					//buzzer_run(LOCK_PULSE_MS);
				}
			}
			break;
		}
	}
}
