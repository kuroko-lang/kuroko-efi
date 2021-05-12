/**
 * @file   efi.c
 * @brief  Kuroko module for interfacing with EFI.
 * @author K. Lange <klange@toaruos.org>
 */
#include <efi.h>
#include <stdio.h>
#include <kuroko/vm.h>
#include <kuroko/object.h>
#include <kuroko/util.h>

extern size_t sbrkHeapSize;

void krkefi_load_module(void) {
	KrkInstance * module = krk_newInstance(vm.baseClasses->moduleClass);
	krk_attachNamedObject(&vm.modules, "efi", (KrkObj*)module);
	krk_attachNamedObject(&module->fields, "__name__", (KrkObj*)S("efi"));
	krk_attachNamedValue(&module->fields, "__file__", NONE_VAL());

	/* Informational stuff */
	krk_attachNamedValue(&module->fields, "heapsize", INTEGER_VAL(sbrkHeapSize));
}


