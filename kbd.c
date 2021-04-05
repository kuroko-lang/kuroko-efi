#include <efi.h>
#include <efilib.h>
#include "kbd.h"

int read_key(void) {
	EFI_INPUT_KEY Key;
	unsigned long int index;
	uefi_call_wrapper(ST->BootServices->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &index);
	uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
	return Key.UnicodeChar;
}
