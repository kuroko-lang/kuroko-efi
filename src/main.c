#include <efi.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "text.h"

#include <kuroko/kuroko.h>
#include <kuroko/vm.h>
#include <kuroko/util.h>

#include "../kuroko/src/vendor/keigo.h"

EFI_HANDLE ImageHandleIn;
EFI_SYSTEM_TABLE *ST;

extern void krk_printResult(unsigned long long val);
extern int krk_repl(void);
extern void krk_repl_debug_hook(void);
extern void free_sbrk_heap(void*);
extern void krkefi_load_module(void);
extern void _createAndBind_gzipMod(void);

static EFI_GUID efi_shell_parameters_protocol_guid = EFI_SHELL_PARAMETERS_PROTOCOL_GUID;
static EFI_GUID efi_simple_text_input_ex_protocol_guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
static EFI_GUID efi_loaded_image_protocol_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

EFI_EVENT efi_timer_event = {};

static EFI_KEY_DATA ctrl_c = {
	{ 0, 'c' }, { 0x80000008, 0 }
};

static EFI_STATUS handle_ctrl_c(EFI_KEY_DATA * data) {
	/* Eat the input if code is executing */
	if (krk_currentThread.frameCount) {
		EFI_INPUT_KEY Key;
		ST->ConIn->ReadKeyStroke(ST->ConIn, &Key);
	}

	ST->BootServices->SetTimer(efi_timer_event, TimerRelative, 0);

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

void abort(void) {
	fprintf(stderr, "Aborting.\n");
	ST->BootServices->Exit(ImageHandleIn, -1, 0, NULL);
	while (1);
}

void exit(int status) {
	ST->BootServices->Exit(ImageHandleIn, status, 0, NULL);
	while (1);
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

static INT32 original_console_mode = 0;
static void reset_attributes(void* unused) {
	ST->ConOut->SetAttribute(ST->ConOut, original_console_mode);
}

static int runString(char * argv[], int flags, char * string) {
	krk_initVM(flags);
	krk_startModule("__main__");
	krk_attachNamedValue(&krk_currentThread.module->fields,"__doc__", NONE_VAL());
	krk_interpret(string, "<stdin>");
	krk_freeVM();
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

	/* See if we have shell arguments */
	EFI_STATUS status;
	EFI_SHELL_PARAMETERS_PROTOCOL * args = NULL;
	status = ST->BootServices->OpenProtocol(
		ImageHandle, &efi_shell_parameters_protocol_guid, (void **)&args,
		ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);

	/* Ensure we free our heap on exit. */
	efi_register_exit_hook(free_sbrk_heap,NULL);

	/* Prepare text output */
	ST->BootServices->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &efi_timer_event);
	original_console_mode = ST->ConOut->Mode->Attribute;
	efi_register_exit_hook(reset_attributes,NULL);
	register_ctrl_callback();
	set_attr(0xF);

	/* Convert arguments */
	char * default_args[] = {"kuroko", NULL};
	int argc = 1;
	char ** argv = default_args;

	if (args) {
		argc = args->Argc;
		argv = calloc(args->Argc + 1, sizeof(char*));
		for (UINTN i = 0; i < args->Argc; ++i) {
			struct StringBuilder sb = {0};
			char buf[7];
			uint64_t ch = 0;
			for (CHAR16 * c = args->Argv[i]; *c; ++c) {
				if (*c >= 0xD800 && *c < 0xDC00) {
					ch = 0x100000 + ((*c - 0xD800) << 10);
					continue;
				} else if (*c >= 0xDC00 && *c < 0xE000) {
					ch += (*c - 0xDC00);
				} else {
					ch = *c;
				}
				int len = to_eight(ch,buf);
				krk_pushStringBuilderStr(&sb, buf, len);
				ch = 0;
			}
			krk_pushStringBuilder(&sb, '\0');
			argv[i] = strdup(sb.bytes);
			krk_discardStringBuilder(&sb);
		}
	}


	/* Normal argument parsing. */
	char * runCmd = NULL;
	int flags = 0;
	int moduleAsMain = 0;
	int inspectAfter = 0;
	int opt;
	int maxDepth = -1;
	struct Keigo ctx = {0};
#define optind (ctx.i)
#define optarg (ctx.arg)
#define optopt (ctx.opt)
	while ((opt = keigo(&ctx,argc,argv,"c:C:dgGim:rR:tTMSV-:")) != -1) {
		switch (opt) {
			case 'c':
				runCmd = optarg;
				goto _finishArgs;
			case 'd':
				/* Disassemble code blocks after compilation. */
				flags |= KRK_THREAD_ENABLE_DISASSEMBLY;
				break;
			case 'g':
				/* Always garbage collect during an allocation. */
				flags |= KRK_GLOBAL_ENABLE_STRESS_GC;
				break;
			case 'G':
				flags |= KRK_GLOBAL_REPORT_GC_COLLECTS;
				break;
			case 'S':
				flags |= KRK_THREAD_SINGLE_STEP;
				break;
			case 't':
				/* Disassemble instructions as they are executed. */
				flags |= KRK_THREAD_ENABLE_TRACING;
				break;
			case 'i':
				inspectAfter = 1;
				break;
			case 'm':
				moduleAsMain = 1;
				optind--; /* to get us back to optarg */
				goto _finishArgs;
			case 'r':
				/* Ignored for compatibility; rline is the only repl input method we have. */
				break;
			case 'R':
				maxDepth = atoi(optarg);
				break;
			case 'M':
				return runString(argv,0,"import kuroko; print(kuroko.module_paths)\n");
			case 'V':
				return runString(argv,0,"import kuroko; print('Kuroko',kuroko.version)\n");
#if 0
			case 'C':
				return compileFile(argv,flags,optarg);
#endif
			case ':':
				fprintf(stderr, "%s: option '%c' requires an argument\n", argv[0], optopt);
				return 1;
			case '?':
				fprintf(stderr, "%s: unrecognized option '%c'\n", argv[0], optopt);
				return 1;
			case '-':
				if (!strcmp(optarg,"version")) {
					return runString(argv,0,"import kuroko; print('Kuroko',kuroko.version)\n");
				} else if (!strcmp(optarg,"help")) {
#ifndef KRK_NO_DOCUMENTATION
					fprintf(stderr,"usage: %s [flags] [FILE...]\n"
						"\n"
						"Interpreter options:\n"
						" -c cmd      Compile and run the string 'cmd'.\n"
						" -d          Debug output from the bytecode compiler.\n"
						" -g          Collect garbage on every allocation.\n"
						" -G          Report GC collections.\n"
						" -i          Enter repl after a running -c, -m, or FILE.\n"
						" -m mod      Run a module as a script.\n"
						" -r          Disable complex line editing in the REPL.\n"
						" -R depth    Set maximum recursion depth.\n"
						" -t          Disassemble instructions as they are exceuted.\n"
//						" -C file     Compile 'file', but do not execute it.\n"
						" -M          Print the default module import paths.\n"
						" -S          Enable single-step debugging.\n"
						" -V          Print version information.\n"
						"\n"
						" --version   Print version information.\n"
						" --help      Show this help text.\n"
						"\n"
						"If no files are provided, the interactive REPL will run.\n",
						argv[0]);
#endif
					return 0;
				}
				fprintf(stderr,"%s: unrecognized option '--%s'\n", argv[0], optarg);
				return 1;
		}
	}

_finishArgs:
	/* Initialize VM */
	krk_initVM(flags);

	if (maxDepth != -1) {
		krk_setMaximumRecursionDepth(maxDepth);
	}

	krk_repl_debug_hook();

	/* Attach kuroko.argv - argv[0] will be set to an empty string for the repl */
	if (argc == optind) krk_push(OBJECT_VAL(krk_copyString("",0)));
	for (int arg = optind; arg < argc; ++arg) {
		krk_push(OBJECT_VAL(krk_copyString(argv[arg],strlen(argv[arg]))));
	}
	KrkValue argList = krk_callNativeOnStack(argc - optind + (optind == argc), &krk_currentThread.stackTop[-(argc - optind + (optind == argc))], 0, krk_list_of);
	krk_push(argList);
	krk_attachNamedValue(&vm.system->fields, "argv", argList);
	krk_pop();
	for (int arg = optind; arg < argc + (optind == argc); ++arg) krk_pop();

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
	krk_push(OBJECT_VAL(krk_copyString(#name, sizeof(#name)-1))); \
	KrkValue moduleOut = krk_module_onload_ ## name ((KrkString*)AS_OBJECT(krk_peek(0))); \
	krk_attachNamedValue(&vm.modules, # name, moduleOut); \
	krk_attachNamedValue(&AS_INSTANCE(moduleOut)->fields, "__file__", NONE_VAL()); \
	krk_pop(); \
} while (0)

	BUNDLED(dis);
	BUNDLED(fileio);

	KrkValue result = INTEGER_VAL(0);

	if (moduleAsMain) {
		int out = !krk_importModule(AS_STRING(AS_LIST(argList)->values[0]), S("__main__"));
		if (krk_currentThread.flags & KRK_THREAD_HAS_EXCEPTION) {
			krk_dumpTraceback();
			krk_resetStack();
		}
		if (!inspectAfter) return out;
		if (IS_INSTANCE(krk_peek(0))) {
			krk_currentThread.module = AS_INSTANCE(krk_peek(0));
		}
	} else if (optind != argc) {
		krk_startModule("__main__");
		result = krk_runfile(argv[optind],argv[optind]);
		if (IS_NONE(result) && krk_currentThread.flags & KRK_THREAD_HAS_EXCEPTION) result = INTEGER_VAL(1);
	}

	if (!krk_currentThread.module) {
		/* The repl runs in the context of a top-level module so each input
		 * line can share a globals state with the others. */
		krk_startModule("__main__");
		krk_attachNamedValue(&krk_currentThread.module->fields,"__doc__", NONE_VAL());
	}

	if (runCmd) {
		result = krk_interpret(runCmd, "<stdin>");
	}

	if ((!moduleAsMain && !runCmd && optind == argc) || inspectAfter) {
		if (vm.system) {
			KrkValue version, buildenv, builddate;
			krk_tableGet_fast(&vm.system->fields, S("version"), &version);
			krk_tableGet_fast(&vm.system->fields, S("buildenv"), &buildenv);
			krk_tableGet_fast(&vm.system->fields, S("builddate"), &builddate);

			fprintf(stdout, "Kuroko %s (%s) with %s\n",
				AS_CSTRING(version), AS_CSTRING(builddate), AS_CSTRING(buildenv));
		}

		puts("Type `license` for copyright, `exit()` to return to menu.");
		krk_repl();
	}

	/* We're returning to EFI, free the resources we used. */
	efi_run_exit_hooks();
	return 0;
}
