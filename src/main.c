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

EFI_STATUS
	EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	ST = SystemTable;
	ImageHandleIn = ImageHandle;

	/*
	 * Disable watchdog
	 */
	ST->BootServices->SetWatchdogTimer(0, 0, 0, NULL);

	/*
	 * Start shell
	 */
	set_attr(0xF);
	krk_initVM(0);

	krkefi_load_module();
	_createAndBind_gzipMod();

	krk_startModule("__main__");
	krk_interpret(
		"if True:\n"
		" import kuroko\n"
		" print(f'Kuroko {kuroko.version} ({kuroko.builddate}) with {kuroko.buildenv}')\n"
		" kuroko.module_paths = ['/krk/','/']\n", "<stdin>");
	puts("Type `license` for copyright, `exit()` to return to menu.");

	krk_repl();

	free_sbrk_heap();

	return 0;
}
