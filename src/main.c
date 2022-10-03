#include <efi.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "text.h"

#include <kuroko/kuroko.h>
#include <kuroko/vm.h>
#include <kuroko/util.h>

EFI_HANDLE ImageHandleIn;
EFI_SYSTEM_TABLE *ST;

extern void krk_printResult(unsigned long long val);
extern int krk_repl(void);
extern void free_sbrk_heap(void);
extern void krkefi_load_module(void);
extern void _createAndBind_gzipMod(void);

static EFI_GUID efi_simple_text_input_ex_protocol_guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;

static EFI_KEY_DATA ctrl_c = {
	{ 0, 'c' }, { 0x80000008, 0 }
};

static EFI_STATUS handle_ctrl_c(EFI_KEY_DATA * data) {
	/* Eat the input if code is executing */
	if (krk_currentThread.frameCount) {
		EFI_INPUT_KEY Key;
		ST->ConIn->ReadKeyStroke(ST->ConIn, &Key);
	}

	/* Set the signalled state */
	krk_currentThread.flags |= KRK_THREAD_SIGNALLED;
	return 0;
}

static int register_ctrl_callback(void) {
	UINTN count;
	EFI_HANDLE * handles;
	EFI_GRAPHICS_OUTPUT_PROTOCOL * gfx;
	EFI_STATUS status = ST->BootServices->LocateHandleBuffer(ByProtocol, &efi_simple_text_input_ex_protocol_guid, NULL, &count, &handles);

	if (EFI_ERROR(status)) {
		return -1;
	}

	for (UINTN i = 0; i < count; ++i) {
		EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL * input_ex;
		void * handle;
		status = ST->BootServices->HandleProtocol(handles[0], &efi_simple_text_input_ex_protocol_guid, (void **)&input_ex);
		if (EFI_ERROR(status)) continue;
		status = input_ex->RegisterKeyNotify(input_ex, &ctrl_c, handle_ctrl_c, &handle);
		if (EFI_ERROR(status)) continue;
	}

	return 0;
}

EFI_STATUS
	EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	ST = SystemTable;
	ImageHandleIn = ImageHandle;

	/* Disable watchdog timer */
	ST->BootServices->SetWatchdogTimer(0, 0, 0, NULL);

	/* Initialize VM */
	set_attr(0xF);
	krk_initVM(0);

	register_ctrl_callback();

	/* Load additional modules */
	krkefi_load_module();
	_createAndBind_gzipMod();

	/* Nothing else to do, start the REPL */
	krk_startModule("__main__");
	krk_interpret(
		"if True:\n"
		" import kuroko\n"
		" print(f'Kuroko {kuroko.version} ({kuroko.builddate}) with {kuroko.buildenv}')\n"
		" kuroko.module_paths = ['/krk/','/']\n", "<stdin>");
	puts("Type `license` for copyright, `exit()` to return to menu.");
	krk_repl();

	/* We're returning to EFI, free the resources we used. */
	free_sbrk_heap();

	return 0;
}
