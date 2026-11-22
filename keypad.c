/**
 * @file :  keypad.c
 * @author : Robert Cromer
 * @brief :  Contains the functions needed to use the keypad
 */


#include "stm32f746xx.h"
#include "stm32f7xx.h"
#include <stdint.h>
#include "keypad.h"


void keypad_idle(void);
void keypad_scan(void);
void keypad_enable_interrupts(void);
void keypad_disable_interrupts(void);


/**
 * The number of rows and columns on the keypad
 */
#define KEYPAD_ROW_COLUMN_NUMBER 4

/**
 * Interrupt priority for keypad
 */
#define KEYPAD_INTERRUPT_PRIORITY (9UL << 4)

/**
 * Keypad EXTI mask/unmask pin expression
 */
#define KEYPAD_EXTI_MASK ((1UL << 0) | (1UL << 8) | (1UL << 9) | (1UL << 10))

/**
 * A character mapping of the keypad
 */
const char keypad_map[KEYPAD_ROW_COLUMN_NUMBER][KEYPAD_ROW_COLUMN_NUMBER] =
		{{'1', '2', '3', 'A'},
		 {'4', '5', '6', 'B'},
		 {'7', '8', '9', 'C'},
		 {'*', '0', '#', 'D'}};


volatile char keypad_character_buffer[CHAR_BUFFER_SIZE];
volatile int keypad_character_buffer_index = 0;


/**
 * A mapping of the rows to the GPIO port they are connected to
 */
GPIO_TypeDef* ROW_GPIO_PORTS[KEYPAD_ROW_COLUMN_NUMBER] = {GPIOF, GPIOF, GPIOF, GPIOA};

/**
 * A mapping of the pin numbers on the GPIO ports form the columns
 */
int ROW_GPIO_PIN_NUMBERS[KEYPAD_ROW_COLUMN_NUMBER] = {8, 9, 10, 0};

/**
 * A mapping of the columns to the GPIO port they are connected to
 */
GPIO_TypeDef* COLUMN_GPIO_PORTS[KEYPAD_ROW_COLUMN_NUMBER] = {GPIOH, GPIOI, GPIOG, GPIOB};

/**
 * A mapping of the columns to the pin number on the connected ports
 */
int COLUMN_GPIO_PIN_NUMBERS[KEYPAD_ROW_COLUMN_NUMBER] = {6, 0, 7, 4};

/**
 * Initializes the following pins for the connections listed below:
 * D3 (B4) -> Keypad pin 1 (column 3)
 * D4 (G7) -> Keypad pin 2 (column 2)
 * D5 (I0) -> Keypad pin 3 (column 1)
 * D6 (H6) -> Keypad pin 4 (column 0)
 * A0 (A0) -> Keypad pin 5 (Row 3)
 * A1 (F10) -> Keypad pin 6 (Row 2)
 * A2 (F9) -> Keypad pin 7 (Row 1)
 * A3 (F8) -> Keypad pin 8 (Row 0)
 *
 * The following interrupts will also be set up to make it so that the rows trigger
 * a keypad read:
 * EXTI0
 * EXTI9
 * EXTI8
 * EXTI10
 */
void keypad_initialize(){
	// Ensures that necessary clocks are enabled
	// 0 for GPIOA, 1 for GPIOB, 5 for GPIOF, 7 for GPIOH, and 8 for GPIOI
	RCC->AHB1ENR |= (1UL << 0) | (1UL << 1) | (1UL << 5) | (1UL << 6) | (1UL << 7) | (1UL << 8);

	// Enables SYSFG clock for interrupts
	RCC->APB2ENR |= (1UL << 14);

	// Configures interrupts for the code

	// Configures EXTI0 to A0
	SYSCFG->EXTICR[0] &= ~(0xFUL);
	EXTI->FTSR |= (1UL);
	EXTI->RTSR &= ~(1UL);
	NVIC->IP[6] = KEYPAD_INTERRUPT_PRIORITY;
	NVIC->ISER[0] = (1 << 6);


	// Configures EXTI9 and EXTI8
	SYSCFG->EXTICR[2] &= ~(0xFFUL);
	SYSCFG->EXTICR[2] |= (0x5UL) | (0x5UL << 4); // multiplexes EXTI8 and EXTI9 to F8 and F9 respectively
	EXTI->FTSR |= (1UL << 8) | (1UL << 9);
	EXTI->RTSR &= ~((1UL << 8) | (1UL << 9));
	NVIC->IP[23] = KEYPAD_INTERRUPT_PRIORITY;
	NVIC->ISER[0] |= (1 << 23);

	// Configures EXTI10
	SYSCFG->EXTICR[2] &= ~(0xFUL << 8);
	SYSCFG->EXTICR[2] |= (5UL << 8);
	EXTI->FTSR |= (1 << 10);
	EXTI->RTSR &= ~(1 << 10);
	NVIC->IP[40] = KEYPAD_INTERRUPT_PRIORITY;
	NVIC->ISER[1] |= (1 << 8);

	// puts keypad in the idle state, which means that the pins are initialized to wait for an interrupt
	// and the interrupts are enabled to run
	keypad_idle();

	EXTI->PR |= KEYPAD_EXTI_MASK;
	keypad_enable_interrupts();
}

/**
 * Sets the column pins to pull-up input and row pins to 0. Because of the way that the
 * MCU pins are set up on the Arduino header, we are going to drive the rows low and
 * configure the columns to listen and receive interrupts. The following pins are configured:
 * D3 (B4) -> output low
 * D4 (G7) -> output low
 * D5 (I0) -> output low
 * D6 (H6) -> output low
 * A0 (A0) -> input pull-up
 * A1 (F10) -> input pull-up
 * A2 (F9) -> input pull-up
 * A3 (F8) -> input pull-up
 */
void keypad_idle(){
	// Sets A0 (A0) to input pull-up
	GPIOA->MODER &= ~(3UL);
	GPIOA->PUPDR &= ~(3UL);
	GPIOA->PUPDR |= 1;

	// Sets A1 (F10) to input pull-up
	GPIOF->MODER &= ~(3UL << (2 * 10));
	GPIOF->PUPDR &= ~(3UL << (2 * 10));
	GPIOF->PUPDR |= (1UL << (2 * 10));

	// Sets A2 (F9) to input pull-up
	GPIOF->MODER &= ~(3UL << (2 * 9));
	GPIOF->PUPDR &= ~(3UL << (2 * 9));
	GPIOF->PUPDR |= (1UL << (2 * 9));

	// Sets A3 (F8) to input pull-up
	GPIOF->MODER &= ~(3UL << (2 * 8));
	GPIOF->PUPDR &= ~(3UL << (2 * 8));
	GPIOF->PUPDR |= (1UL << (2 * 8));

	// Sets D3 (B4) to output low
	GPIOB->MODER &= ~(3UL << (2 * 4));
	GPIOB->MODER |= (1UL << (2 * 4));
	GPIOB->BSRR |= (1 << (16 + 4));

	// Sets D4 (G7) to output low
	GPIOG->MODER &= ~(3UL << (2 * 7));
	GPIOG->MODER |= (1UL << (2 * 7));
	GPIOG->BSRR |= (1 << (16 + 7));

	// Sets D5 (I0) to output low
	GPIOI->MODER &= ~3UL;
	GPIOI->MODER |= (1UL);
	GPIOI->BSRR |= (1 << (16));

	// Sets D6 (H6) to output low
	GPIOH->MODER &= ~(3UL << (6 * 2));
	GPIOH->MODER |= (1UL << (2 * 6));
	GPIOH->BSRR |= (1 << (6 + 16));

}

/**
 * Sets the keypad pins to scan the keypad with the first More specifically, the following are set:
 * D3 (B4) -> input pull-up
 * D4 (G7) -> input pull-up
 * D5 (I0) -> input pull-up
 * D6 (H6) -> input pull-up
 * A0 (A0) -> output high
 * A1 (F10) -> output high
 * A2 (F9) -> output high
 * A3 (F8) -> output high
 */
void keypad_activate(){

	keypad_disable_interrupts();

	// Sets D3 (B4) to input pull-up
	GPIOB->MODER &= ~(3UL << (2 * 4));
	GPIOB->PUPDR &= ~(3UL << (2 * 4));
	GPIOB->PUPDR |= (1UL << (2 * 4));

	// Sets D4 (G7) to input pull-up
	GPIOG->MODER &= ~(3UL << (2 * 7));
	GPIOG->PUPDR &= ~(3UL << (2 * 7));
	GPIOG->PUPDR |= (1UL << 2 * 7);

	// Sets D5 (I0) to input pull-up
	GPIOI->MODER &= ~3UL;
	GPIOI->PUPDR &= ~3UL;
	GPIOI->PUPDR |= (1UL);

	// Sets D6 (H6) to input pull-up
	GPIOH->MODER &= ~(3UL << (2 * 6));
	GPIOH->PUPDR &= ~(3UL << (2 * 6));
	GPIOH->PUPDR |= (1 << (2 * 6));

	// Sets A0 (A0) to output high
	GPIOA->MODER &= ~(3UL);
	GPIOA->MODER |= 1UL;
	GPIOA->BSRR |= 1UL;

	// Sets A1 (F10) to output high
	GPIOF->MODER &= ~(3UL << (2 * 10));
	GPIOF->MODER |= (1UL << (2 * 10));
	GPIOF->BSRR |= (1 << 10);

	// Sets A2 (F9) to output high
	GPIOF->MODER &= ~(3UL << (2 * 9));
	GPIOF->MODER |= (1UL << (2 * 9));
	GPIOF->BSRR |= (1 << 9);

	// Sets A3 (F8) to output high
	GPIOF->MODER &= ~(3UL << (2 * 8));
	GPIOF->MODER |= (1UL << (2 * 8));
	GPIOF->BSRR |= (1 << 8);
}


void keypad_scan(void){
	char scanned_character = 0;
	keypad_activate();
	for (volatile int d = 0; d < 500; d++) __asm("NOP");     // settle

	for (int i = 0; i < KEYPAD_ROW_COLUMN_NUMBER; i++){
		ROW_GPIO_PORTS[i]->BSRR = (1UL << (ROW_GPIO_PIN_NUMBERS[i] + 16));  // row low
		for (volatile int d = 0; d < 500; d++);
		for (int j = 0; j < KEYPAD_ROW_COLUMN_NUMBER; j++){
			if (!((COLUMN_GPIO_PORTS[j]->IDR >> COLUMN_GPIO_PIN_NUMBERS[j]) & 1UL)){
				scanned_character = keypad_map[i][j];
				break;
			}
		}
		ROW_GPIO_PORTS[i]->BSRR = (1UL << ROW_GPIO_PIN_NUMBERS[i]);         // row high
		if (scanned_character) break;
	}

	if (scanned_character && keypad_character_buffer_index < CHAR_BUFFER_SIZE){
		keypad_character_buffer[keypad_character_buffer_index++] = scanned_character;
	}
}


static void keypad_handle_press(void){
	keypad_disable_interrupts();
	keypad_scan();                  // masks EXTI via keypad_activate()
	keypad_idle();                  // restore pins (no longer enables EXTI)
	EXTI->PR = KEYPAD_EXTI_MASK;    // clear edges caused by the scan
	keypad_enable_interrupts();
}

void EXTI0_IRQHandler(void){     keypad_handle_press(); }
void EXTI9_5_IRQHandler(void){   keypad_handle_press(); }
void EXTI15_10_IRQHandler(void){ keypad_handle_press(); }

void keypad_enable_interrupts(void){
	EXTI->IMR |= KEYPAD_EXTI_MASK;
}

void keypad_disable_interrupts(void){
	EXTI->IMR &= ~KEYPAD_EXTI_MASK;
}
