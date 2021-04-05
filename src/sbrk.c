#include <efi.h>
#include <efilib.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#define MAX_PAGES 128000

static char * base = NULL;
static char * endp = NULL;
static char * curr = NULL;

void * sbrk(size_t bytes) {
	if (!base) {
		EFI_PHYSICAL_ADDRESS allocSpace;
		size_t tryPages = MAX_PAGES;
		for (;;) {
			EFI_STATUS status = uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, tryPages, &allocSpace);
			if (!EFI_ERROR(status)) {
				break;
			}
			tryPages >> 1;
		}
		base = (char *)(intptr_t)allocSpace;
		endp = base + tryPages * 0x1000;
		curr = base;
	}

	if (curr + bytes >= endp) {
		printf("Error: Ran out of pages (%p + %d >= %p)\n", (void*)curr, (int)bytes, (void*)endp);
		while (1) {}
		return NULL;
	}

	void * out = curr;
	memset(out, 0x00, bytes);
	curr += bytes;
	return out;
}

