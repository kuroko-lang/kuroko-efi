#include <efi.h>
#include <stdio.h>
#include <kuroko/vm.h>
#include <kuroko/object.h>
#include <kuroko/util.h>

extern EFI_SYSTEM_TABLE *ST;

KRK_Function(uname) {
	KrkValue result = krk_dict_of(0, NULL, 0);
	krk_push(result);

	/* UEFI version information */
	krk_attachNamedObject(AS_DICT(result), "sysname",  (KrkObj*)S("UEFI"));

	char tmp[20];
	size_t len = snprintf(tmp,20,"%d.%02d",
		((ST->Hdr.Revision) & 0xFFFF0000) >> 16,
		((ST->Hdr.Revision) & 0x0000FFFF));

	krk_attachNamedObject(AS_DICT(result), "release",  (KrkObj*)krk_copyString(tmp,len));

	/* Firmware information */
	struct StringBuilder sb = {0};
	uint16_t * fw = ST->FirmwareVendor;
	while (*fw) {
		pushStringBuilder(&sb, *fw);
		fw++;
	}
	krk_attachNamedValue(AS_DICT(result), "nodename", finishStringBuilder(&sb));

	len = snprintf(tmp, 20, "%x", ST->FirmwareRevision);
	krk_attachNamedObject(AS_DICT(result), "version",  (KrkObj*)krk_copyString(tmp,len));

	krk_attachNamedObject(AS_DICT(result), "machine",  (KrkObj*)S(
#ifdef __x86_64__
	"x86_64"
#elif defined(__aarch64__)
	"aarch64"
#else
	"unknown"
#endif
	));

	return krk_pop();
}

extern EFI_HANDLE ImageHandleIn;

extern void efi_run_exit_hooks(void);

KRK_Function(exit) {
	efi_run_exit_hooks();
	ST->BootServices->Exit(ImageHandleIn, EFI_SUCCESS, 0, NULL);
	__builtin_unreachable();
}

void krk_module_init_os(void) {
	KrkInstance * module = krk_newInstance(vm.baseClasses->moduleClass);
	krk_attachNamedObject(&vm.modules, "os", (KrkObj*)module);
	krk_attachNamedObject(&module->fields, "__name__", (KrkObj*)S("os"));
	krk_attachNamedValue(&module->fields, "__file__", NONE_VAL());
	BIND_FUNC(module,uname);

	BIND_FUNC(vm.builtins,exit);
}


