/**
 * @author: Robert Cromer
 * @file : keypad.h
 * @brief : contains prototypes for the keypad library
 *
 */

#ifndef KEYPAD
#define KEYPAD

/**
 * The size of the characer buffer
 */
#define CHAR_BUFFER_SIZE 10

/**
 * An array to hold all of the characters read
 */
extern volatile char keypad_character_buffer[CHAR_BUFFER_SIZE];
extern volatile int keypad_character_buffer_index;

void keypad_initialize();
void keypad_idle(void);
void keypad_scan(void);

#endif
