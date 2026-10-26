#ifndef UL_KEYBOARD_H
#define UL_KEYBOARD_H
#include "types.h"
enum { KEY_UP = 0x101, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_HOME, KEY_END, KEY_DEL, KEY_PGUP, KEY_PGDN };
void keyboard_init(void);
int  keyboard_poll(void);   /* -1 if empty, else ASCII or KEY_* */
#endif
