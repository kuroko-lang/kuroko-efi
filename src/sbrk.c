#include <efi.h>
#include <efilib.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#define MAX_PAGES 128000

static char * base = NULL;
static char * endp = NULL;
static char * curr = NULL;

size_t sbrkHeapSize = 0;

void * sbrk(size_t bytes) {
	if (!base) {
		EFI_PHYSICAL_ADDRESS allocSpace;
		size_t tryPages = MAX_PAGES;
		for (;;) {
			EFI_STATUS status = uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, tryPages, &allocSpace);
			if (!EFI_ERROR(status)) {
				break;
			}
			tryPages >>= 1;
			if (tryPages == 0) {
				printf("Unable to obtain space for a heap, giving up.");
				while (1);
			}
		}
		base = (char *)(intptr_t)allocSpace;
		endp = base + tryPages * 0x1000;
		curr = base;
		sbrkHeapSize = endp - base;
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

void free_sbrk_heap(void) {
	if (base) {
		EFI_PHYSICAL_ADDRESS allocSpace = (uint64_t)(uintptr_t)base;
		size_t pages = (endp - base) / 0x1000;
		uefi_call_wrapper(ST->BootServices->FreePages, 2, allocSpace, pages);
	}
}
