/*
 * sys_main_ps4.c - PS4 application entry point and main loop
 *
 * Replaces code/sys/sys_main.c for the PS4 platform.
 * Initializes PS4 system services, then runs the Quake 3 engine.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <orbis/CommonDialog.h>
#include <orbis/MsgDialog.h>
#include <orbis/Pigletv2VSH.h>
#include <orbis/VideoOut.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../sys/sys_local.h"

// Forward declarations for console backend
void CON_Init(void);
void CON_Print(const char *msg);
void CON_Shutdown(void);

// sceKernelReserveVirtualRange is not declared in OpenOrbis headers
int sceKernelReserveVirtualRange(void **addr, size_t len, int flags, size_t alignment);
// sceKernelMapNamedSystemFlexibleMemory IS in libkernel.h but with void return/no params.
// We cast the call to int via a wrapper to avoid the conflicting declaration.

/*
 * Sys_Init
 */
void Sys_Init(void)
{
	Cvar_Set("arch", ARCH_STRING);
}

/*
 * Sys_Quit
 */
void Sys_Quit(void)
{
	Com_Shutdown();
	CON_Shutdown();
	sceSystemServiceLoadExec("exit", NULL);
	exit(0);
}

/*
 * Sys_Error
 */
void Sys_Error(const char *error, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, error);
	Q_vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	Com_Printf("Sys_Error: %s\n", string);
	Sys_ErrorDialog(string);
	Sys_Quit();
}

/*
 * Sys_Print
 */
void Sys_Print(const char *msg)
{
	CON_Print(msg);
}

/*
 * Sys_ErrorDialog
 *
 * Show a PS4 system message dialog with the error text.
 * This blocks until the user presses OK, so the error is visible.
 */
void Sys_ErrorDialog(const char *error)
{
	Com_Printf("ERROR: %s\n", error);

	// Initialize common dialog system
	sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG);
	sceMsgDialogInitialize();

	// Set up user message param
	OrbisMsgDialogUserMessageParam userMsgParam;
	memset(&userMsgParam, 0, sizeof(userMsgParam));
	userMsgParam.buttonType = ORBIS_MSG_DIALOG_BUTTON_TYPE_OK;
	userMsgParam.msg = error;

	// Set up dialog param
	OrbisMsgDialogParam param;
	memset(&param, 0, sizeof(param));
	param.baseParam.size = sizeof(param.baseParam);
	param.baseParam.magic = ORBIS_COMMON_DIALOG_MAGIC_NUMBER;
	param.size = sizeof(param);
	param.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;
	param.userMsgParam = &userMsgParam;

	// Open dialog and wait for user to dismiss
	if (sceMsgDialogOpen(&param) == 0) {
		OrbisCommonDialogStatus status;
		while (1) {
			status = sceMsgDialogUpdateStatus();
			if (status == ORBIS_COMMON_DIALOG_STATUS_FINISHED)
				break;
			sceKernelUsleep(16000); // ~60fps polling
		}
		sceMsgDialogClose();
	}

	sceMsgDialogTerminate();
}

/*
 * PS4_LoadSystemModules
 *
 * Load required system modules using sceKernelLoadStartModule from the
 * sandbox path -- exactly as the SM64 PS4 port does. This is the only
 * proven working method on retail FW 9.00 with GoldHEN.
 *
 * SM64 loads: SysCore, Mbus, Ipmi, SystemService, UserService, AudioOut, Pad
 * We add: Net, NetCtl (needed for networking)
 *
 * NOTE: SM64 does NOT use sceSysmoduleLoadModuleInternal at all.
 * NOTE: SM64 does NOT load VideoOut or PrecompiledShaders.
 */
static void PS4_LoadSystemModules(void)
{
	const char *sandboxWord;
	char path[256];
	int ret, handle;
	int i;

	// Exact SM64 module list, in order
	const char *modules[] = {
		"libSceSysCore",
		"libSceMbus",
		"libSceIpmi",
		"libSceSystemService",
		"libSceUserService",
		"libSceAudioOut",
		"libScePad",
		// Additional modules we need:
		"libSceNet",
		"libSceNetCtl",
		"libSceVideoOut",
		"libSceImeDialog"
	};
	int numModules = sizeof(modules) / sizeof(modules[0]);

	sandboxWord = sceKernelGetFsSandboxRandomWord();
	if (sandboxWord) {
		snprintf(path, sizeof(path), "/%s/common/lib", sandboxWord);
	} else {
		snprintf(path, sizeof(path), "/system/common/lib");
	}

	for (i = 0; i < numModules; i++) {
		char fullpath[256];
		char msg[256];
		snprintf(fullpath, sizeof(fullpath), "%s/%s.sprx", path, modules[i]);
		handle = sceKernelLoadStartModule(fullpath, 0, NULL, 0, NULL, &ret);
		if (handle >= 0) {
			snprintf(msg, sizeof(msg), "  sys: %s (handle=0x%X)\n", modules[i], handle);
		} else {
			snprintf(msg, sizeof(msg), "  sys: %s FAILED: 0x%08X\n", modules[i], handle);
		}
		CON_Print(msg);
	}
}

/*
 * PS4 main entry point
 */
int main(int argc, char **argv)
{
	char commandLine[MAX_STRING_CHARS] = {0};
	int i;

	// Initialize console output FIRST so we can log
	CON_Init();

	// Init mspace first (malloc must work for rest of engine)
	{
		extern int malloc_init(void);
		extern int malloc_get_debug(int*, int*, void**, size_t*);
		int mi = malloc_init();
		int r,m; void *b; size_t s;
		malloc_get_debug(&r, &m, &b, &s);
		char buf[128];
		snprintf(buf, sizeof(buf), "malloc=%d size=%zuMB\n", mi, s/(1024*1024));
		CON_Print(buf);
	}

	// Load system modules via sandbox path
	PS4_LoadSystemModules();

	// Hide splash screen
	sceSystemServiceHideSplashScreen();

	// Initialize UserService
	{
		OrbisUserServiceInitializeParams userParam;
		memset(&userParam, 0, sizeof(userParam));
		userParam.priority = 700;
		sceUserServiceInitialize(&userParam);
	}

	// Build command line from argv
	for (i = 1; i < argc; i++) {
		if (i > 1) {
			Q_strcat(commandLine, sizeof(commandLine), " ");
		}
		Q_strcat(commandLine, sizeof(commandLine), argv[i]);
	}

	// Append a +set arg, space-separated only if the buffer is non-empty.
	// A leading space before the first '+' creates a spurious com_consoleLines[0]
	// entry ("  ") that doesn't match the "set " skip filter, causing
	// Com_AddStartupCommands() to return qtrue and block cinematic playback.
#define PS4_ADDARG(buf, sz, arg) \
	do { if ((buf)[0]) Q_strcat(buf, sz, " "); Q_strcat(buf, sz, arg); } while(0)

	// Set default game data path
	if (!strstr(commandLine, "+set fs_basepath"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set fs_basepath /app0");
	if (!strstr(commandLine, "+set fs_homepath"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set fs_homepath /data/ioq3");

	// Force GLES-friendly settings
	if (!strstr(commandLine, "+set r_preferOpenGLES"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set r_preferOpenGLES 1");

	// Force game modules to use interpreted VM (safest for initial port)
	if (!strstr(commandLine, "+set vm_game"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set vm_game 1");
	if (!strstr(commandLine, "+set vm_cgame"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set vm_cgame 1");
	if (!strstr(commandLine, "+set vm_ui"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set vm_ui 1");

#undef PS4_ADDARG

	Com_Printf("ioQuake3 PS4 Port\n");
	Com_Printf("Platform: %s\n", PLATFORM_STRING);

	// Initialize engine (this calls GLimp_Init, which creates EGL context)
	Sys_PlatformInit();
	
	extern void PS4_NetInit(void);
	PS4_NetInit();
	
	Com_Init(commandLine);

	NET_Init();

	// Main loop
	while (1) {
		Com_Frame();
	}

	return 0;
}
