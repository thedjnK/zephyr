/*
 * Copyright (c) 2023 Jamie McCrae
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_AUXDISPLAY_HD44780_
#define H_AUXDISPLAY_HD44780_

#define AUXDISPLAY_HD44780_BACKLIGHT_MIN 0
#define AUXDISPLAY_HD44780_BACKLIGHT_MAX 1

#define AUXDISPLAY_HD44780_CUSTOM_CHARACTERS 8
#define AUXDISPLAY_HD44780_CUSTOM_CHARACTER_WIDTH 5
#define AUXDISPLAY_HD44780_CUSTOM_CHARACTER_HEIGHT 8

enum {
	AUXDISPLAY_HD44780_MODE_4_BIT = 0,
	AUXDISPLAY_HD44780_MODE_8_BIT = 1,

	/* Reserved for internal driver use only */
	AUXDISPLAY_HD44780_MODE_4_BIT_ONCE,
};

#endif /* H_AUXDISPLAY_HD44780_ */
