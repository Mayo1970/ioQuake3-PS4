/* sys_main_ps4.c -- PS4 entry point. Replaces code/sys/sys_main.c. */

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

void CON_Init(void);
void CON_Print(const char *msg);
void CON_Shutdown(void);

/* Not declared in OpenOrbis headers. */
int sceKernelReserveVirtualRange(void **addr, size_t len, int flags, size_t alignment);

void Sys_Init(void)
{
	Cvar_Set("arch", ARCH_STRING);
}

void Sys_Quit(void)
{
	Com_Shutdown();
	CON_Shutdown();
	sceSystemServiceLoadExec("exit", NULL);
	exit(0);
}

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

void Sys_Print(const char *msg)
{
	CON_Print(msg);
}

/* Blocking dialog; ensures the error is visible before the app exits. */
void Sys_ErrorDialog(const char *error)
{
	Com_Printf("ERROR: %s\n", error);

	sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG);
	sceMsgDialogInitialize();

	OrbisMsgDialogUserMessageParam userMsgParam;
	memset(&userMsgParam, 0, sizeof(userMsgParam));
	userMsgParam.buttonType = ORBIS_MSG_DIALOG_BUTTON_TYPE_OK;
	userMsgParam.msg = error;

	OrbisMsgDialogParam param;
	memset(&param, 0, sizeof(param));
	param.baseParam.size = sizeof(param.baseParam);
	param.baseParam.magic = ORBIS_COMMON_DIALOG_MAGIC_NUMBER;
	param.size = sizeof(param);
	param.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;
	param.userMsgParam = &userMsgParam;

	if (sceMsgDialogOpen(&param) == 0) {
		OrbisCommonDialogStatus status;
		while (1) {
			status = sceMsgDialogUpdateStatus();
			if (status == ORBIS_COMMON_DIALOG_STATUS_FINISHED)
				break;
			sceKernelUsleep(16000);
		}
		sceMsgDialogClose();
	}

	sceMsgDialogTerminate();
}

/* Load sprx modules via the sandbox path (proven working on retail FW 9.00). */
static void PS4_LoadSystemModules(void)
{
	const char *sandboxWord;
	char path[256];
	int ret, handle;
	int i;

	const char *modules[] = {
		"libSceSysCore",
		"libSceMbus",
		"libSceIpmi",
		"libSceSystemService",
		"libSceUserService",
		"libSceAudioOut",
		"libScePad",
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

int main(int argc, char **argv)
{
	char commandLine[MAX_STRING_CHARS] = {0};
	int i;

	CON_Init();

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

	PS4_LoadSystemModules();
	sceSystemServiceHideSplashScreen();

	{
		OrbisUserServiceInitializeParams userParam;
		memset(&userParam, 0, sizeof(userParam));
		userParam.priority = 700;
		sceUserServiceInitialize(&userParam);
	}

	for (i = 1; i < argc; i++) {
		if (i > 1) {
			Q_strcat(commandLine, sizeof(commandLine), " ");
		}
		Q_strcat(commandLine, sizeof(commandLine), argv[i]);
	}

	/* Leading space before the first '+' creates a spurious com_consoleLines[0]
	   that blocks cinematic playback. Only insert the separator when non-empty. */
#define PS4_ADDARG(buf, sz, arg) \
	do { if ((buf)[0]) Q_strcat(buf, sz, " "); Q_strcat(buf, sz, arg); } while(0)

	if (!strstr(commandLine, "+set fs_basepath"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set fs_basepath /app0");
	if (!strstr(commandLine, "+set fs_homepath"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set fs_homepath /data/ioq3");

#ifdef STANDALONETA
	/* TA is a mod layered over baseq3, not a standalone game. */
	if (!strstr(commandLine, "+set fs_game"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set fs_game missionpack");
#endif

	/* Default player name = PSN username. */
	if (!strstr(commandLine, "+set name") && !strstr(commandLine, "+seta name")) {
		OrbisUserServiceUserId userId = -1;
		sceUserServiceGetInitialUser(&userId);
		if (userId >= 0) {
			char psName[ORBIS_USER_SERVICE_MAX_USER_NAME_LENGTH + 1];
			memset(psName, 0, sizeof(psName));
			if (sceUserServiceGetUserName(userId, psName, sizeof(psName)) == 0 && psName[0]) {
				char nameArg[64];
				snprintf(nameArg, sizeof(nameArg), "+set name \"%s\"", psName);
				PS4_ADDARG(commandLine, sizeof(commandLine), nameArg);
			}
		}
	}

	if (!strstr(commandLine, "+set r_preferOpenGLES"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set r_preferOpenGLES 1");

	/* Interpreted VM only; JIT path is untested on PS4. */
	if (!strstr(commandLine, "+set vm_game"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set vm_game 1");
	if (!strstr(commandLine, "+set vm_cgame"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set vm_cgame 1");
	if (!strstr(commandLine, "+set vm_ui"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set vm_ui 1");

#undef PS4_ADDARG

	Com_Printf("ioQuake3 PS4 Port\n");
	Com_Printf("Platform: %s\n", PLATFORM_STRING);

	Sys_PlatformInit();

	extern void PS4_NetInit(void);
	PS4_NetInit();

	Com_Init(commandLine);
	NET_Init();

	while (1) {
		Com_Frame();
	}

	return 0;
}
