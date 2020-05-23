#ifndef CHARS_H
#define CHARS_H

#define CHAR_P_0 0b00001000
#define CHAR_P_1 0b01001001
#define CHAR_P_2 0b01011001
#define CHAR_P_3 0b00011001
#define CHAR_P_4 0b01101001
#define CHAR_P_5 0b01111001
#define CHAR_P_6 0b00111001
#define CHAR_P_7 0b01101000
#define CHAR_P_8 0b01111000
#define CHAR_P_9 0b00111000

#define CHAR_P_SEP 0b00000001

#define CHAR_P_BAR 0b00000100

#define CHAR_P_ERROR 0b00000001

const unsigned char digits_paw[] = {
	CHAR_P_0,
	CHAR_P_1,
	CHAR_P_2,
	CHAR_P_3,
	CHAR_P_4,
	CHAR_P_5,
	CHAR_P_6,
	CHAR_P_7,
	CHAR_P_8,
	CHAR_P_9,
};

#endif