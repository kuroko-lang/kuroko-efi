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
extern void free_sbrk_heap(void*);
extern void krkefi_load_module(void);
extern void _createAndBind_gzipMod(void);

static EFI_GUID efi_shell_parameters_protocol_guid = EFI_SHELL_PARAMETERS_PROTOCOL_GUID;
static EFI_GUID efi_simple_text_input_ex_protocol_guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
static EFI_GUID efi_loaded_image_protocol_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

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

static void ** callback_handles = NULL;

struct ExitHook {
	void (*callback)(void *);
	void * data;
	struct ExitHook * previous;
};

static struct ExitHook * tail = NULL;

void efi_register_exit_hook(void (*callback)(void *), void * data) {
	struct ExitHook * new = malloc(sizeof(struct ExitHook));
	new->callback = callback;
	new->data = data;
	new->previous = tail;
	tail = new;
}

void efi_run_exit_hooks(void) {
	struct ExitHook * cur = tail;
	while (cur) {
		struct ExitHook * next = cur->previous;
		cur->callback(cur->data);
		cur = next;
	}
}

static void unregister_callback(void * data) {
	void ** pair = data;
	EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL * input_ex = pair[0];
	input_ex->UnregisterKeyNotify(input_ex, pair[1]);
}

static int register_ctrl_callback(void) {
	UINTN count;
	EFI_HANDLE * handles;
	EFI_STATUS status = ST->BootServices->LocateHandleBuffer(ByProtocol, &efi_simple_text_input_ex_protocol_guid, NULL, &count, &handles);
	if (EFI_ERROR(status)) return -1;

	callback_handles = calloc(sizeof(void*), count);
	for (UINTN i = 0; i < count; ++i) {
		EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL * input_ex;
		void * handle;
		status = ST->BootServices->HandleProtocol(handles[i], &efi_simple_text_input_ex_protocol_guid, (void **)&input_ex);
		if (EFI_ERROR(status)) continue;
		status = input_ex->RegisterKeyNotify(input_ex, &ctrl_c, handle_ctrl_c, &handle);
		void ** pair = malloc(sizeof(void*) * 2);
		pair[0] = input_ex;
		pair[1] = handle;
		efi_register_exit_hook(unregister_callback, pair);
	}

	return 0;
}

static int to_eight(uint32_t codepoint, char * out) {
	memset(out, 0x00, 7);

	if (codepoint < 0x0080) {
		out[0] = (char)codepoint;
	} else if (codepoint < 0x0800) {
		out[0] = 0xC0 | (codepoint >> 6);
		out[1] = 0x80 | (codepoint & 0x3F);
	} else if (codepoint < 0x10000) {
		out[0] = 0xE0 | (codepoint >> 12);
		out[1] = 0x80 | ((codepoint >> 6) & 0x3F);
		out[2] = 0x80 | (codepoint & 0x3F);
	} else if (codepoint < 0x200000) {
		out[0] = 0xF0 | (codepoint >> 18);
		out[1] = 0x80 | ((codepoint >> 12) & 0x3F);
		out[2] = 0x80 | ((codepoint >> 6) & 0x3F);
		out[3] = 0x80 | ((codepoint) & 0x3F);
	} else if (codepoint < 0x4000000) {
		out[0] = 0xF8 | (codepoint >> 24);
		out[1] = 0x80 | (codepoint >> 18);
		out[2] = 0x80 | ((codepoint >> 12) & 0x3F);
		out[3] = 0x80 | ((codepoint >> 6) & 0x3F);
		out[4] = 0x80 | ((codepoint) & 0x3F);
	} else {
		out[0] = 0xF8 | (codepoint >> 30);
		out[1] = 0x80 | ((codepoint >> 24) & 0x3F);
		out[2] = 0x80 | ((codepoint >> 18) & 0x3F);
		out[3] = 0x80 | ((codepoint >> 12) & 0x3F);
		out[4] = 0x80 | ((codepoint >> 6) & 0x3F);
		out[5] = 0x80 | ((codepoint) & 0x3F);
	}

	return strlen(out);
}

EFI_STATUS
	EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	ST = SystemTable;
	ImageHandleIn = ImageHandle;

	/* Disable watchdog timer */
	ST->BootServices->SetWatchdogTimer(0, 0, 0, NULL);

	/* See if we have shell arguments */
	EFI_STATUS status;
	EFI_SHELL_PARAMETERS_PROTOCOL * args = NULL;
	status = ST->BootServices->OpenProtocol(
		ImageHandle, &efi_shell_parameters_protocol_guid, (void **)&args,
		ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);

	efi_register_exit_hook(free_sbrk_heap,NULL);

	/* Initialize VM */
	set_attr(0xF);
	krk_initVM(0);

	register_ctrl_callback();

	/* Load additional modules */
	krkefi_load_module();
	_createAndBind_gzipMod();

	extern void krk_module_init_os(void);
	krk_module_init_os();
	extern void krk_module_init_time(void);
	krk_module_init_time();

	/* Include dis */
#define BUNDLED(name) do { \
	extern KrkValue krk_module_onload_ ## name (KrkString*); \
	KrkValue moduleOut = krk_module_onload_ ## name (NULL); \
	krk_attachNamedValue(&vm.modules, # name, moduleOut); \
	krk_attachNamedObject(&AS_INSTANCE(moduleOut)->fields, "__name__", (KrkObj*)krk_copyString(#name, sizeof(#name)-1)); \
	krk_attachNamedValue(&AS_INSTANCE(moduleOut)->fields, "__file__", NONE_VAL()); \
} while (0)

	BUNDLED(dis);
	BUNDLED(fileio);

	/* Nothing else to do, start the REPL */
	krk_startModule("__main__");

	KrkValue argList = NONE_VAL();
	if (args && args->Argc > 1) {
		/* We don't support typical options, just potentially a script to run and arguments to pass to it */
		for (UINTN i = 1; i < args->Argc; ++i) {
			struct StringBuilder sb = {0};
			/* TODO: This is bad UCS-2 conversion, not UTF-16 */
			for (CHAR16 * c = args->Argv[i]; *c; ++c) {
				char buf[7];
				int len = to_eight(*c,buf);
				krk_pushStringBuilderStr(&sb, buf, len);
			}
			krk_push(krk_finishStringBuilder(&sb));
		}
		/* Build kuroko.argv from the strings we just pushed */
		argList = krk_callNativeOnStack(args->Argc - 1, &krk_currentThread.stackTop[-(args->Argc - 1)], 0, krk_list_of);
		krk_push(argList);
		krk_attachNamedValue(&vm.system->fields, "argv", argList);
		krk_pop();
		for (int arg = 0; arg < args->Argc - 1; ++arg) krk_pop();
	}

	/* Get loaded image data directly as UTF-16. */
	EFI_LOADED_IMAGE * loaded_image = NULL;
	status = ST->BootServices->HandleProtocol(ImageHandleIn, &efi_loaded_image_protocol_guid, (void**)&loaded_image);
	if (loaded_image && loaded_image->LoadOptionsSize) {
		struct StringBuilder sb = {0};
		/* TODO: This is bad UCS-2 conversion, not UTF-16 */
		for (size_t i = 0; i < loaded_image->LoadOptionsSize; ++i) {
			krk_pushStringBuilder(&sb, ((uint8_t*)loaded_image->LoadOptions)[i]);
		}
		krk_push(krk_finishStringBuilderBytes(&sb));
		krk_attachNamedValue(&vm.system->fields, "efi_LoadOptions", krk_peek(0));
		krk_pop();
	}

	if (!IS_NONE(argList)) {
		/* Run argv[1] */
		krk_runfile(AS_CSTRING(AS_LIST(argList)->values[0]),AS_CSTRING(AS_LIST(argList)->values[0]));
	} else {
		krk_interpret(
			"if True:\n"
			" import kuroko\n"
			" print(f'Kuroko {kuroko.version} ({kuroko.builddate}) with {kuroko.buildenv}')\n"
			" kuroko.module_paths = ['/krk/','/']\n", "<stdin>");
		puts("Type `license` for copyright, `exit()` to return to menu.");
		krk_repl();
	}

	/* We're returning to EFI, free the resources we used. */
	efi_run_exit_hooks();
	return 0;
}
