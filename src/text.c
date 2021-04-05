#include <efi.h>
#include <efilib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "text.h"

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

void print_(char * str) {
	while (*str) {
		if (*str == '\n') print_("\r");
		uint16_t string[] = {bad_ucs2(*str), 0};
		uefi_call_wrapper(ST->ConOut->OutputString, 2, ST->ConOut, string);
		str++;
	}
}

void print_wchar(int wch) {
	uint16_t string[] = {wch, 0};
	uefi_call_wrapper(ST->ConOut->OutputString, 2, ST->ConOut, string);
}

void move_cursor_x(int _x) {
	uefi_call_wrapper(ST->ConOut->SetCursorPosition, 3, ST->ConOut,
		_x, ST->ConOut->Mode->CursorRow);
}

void set_attr(int attr) {
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, attr);
}

void clear_() {
	uefi_call_wrapper(ST->ConOut->SetCursorPosition, 3, ST->ConOut, 0, 0);
	uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
}

