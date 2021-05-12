#include <efi.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "text.h"

extern EFI_SYSTEM_TABLE *ST;

extern uint32_t decode(uint32_t* state, uint32_t* codep, uint32_t byte);

static unsigned short bad_ucs2(int c) {
	switch (c) {
		case '\030':
			return L'↑';
		case '\031':
			return L'↓';
		case '\032':
			return L'←';
		case '\033':
			return L'→';
		default:
			return c;
	}
}

static uint32_t istate = 0, c = 0;
void print_(unsigned char * str) {
	while (*str) {
		if (!decode(&istate, &c, *str)) {
			if (c == '\n') {
				uint16_t string[] = {'\r', 0};
				ST->ConOut->OutputString(ST->ConOut, string);
			}
			uint16_t string[] = {c, 0};
			ST->ConOut->OutputString(ST->ConOut, string);
		}
		str++;
	}
}

void print_wchar(int wch) {
	uint16_t string[] = {wch, 0};
	ST->ConOut->OutputString(ST->ConOut, string);
}

void move_cursor_x(int _x) {
	ST->ConOut->SetCursorPosition(ST->ConOut,
		_x, ST->ConOut->Mode->CursorRow);
}

void set_attr(int attr) {
	ST->ConOut->SetAttribute(ST->ConOut, attr);
}

void clear_() {
	ST->ConOut->SetCursorPosition(ST->ConOut, 0, 0);
	ST->ConOut->ClearScreen(ST->ConOut);
}

