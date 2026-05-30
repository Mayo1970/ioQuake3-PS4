/* sys_main_ps4.c -- PS4 entry point. Replaces code/sys/sys_main.c. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

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
void PS4_ApplyDefaultBindings(void);

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

static void PS4_CopyFile(const char *src, const char *dst)
{
	FILE *in, *out;
	char buf[4096];
	size_t n;

	in = fopen(src, "rb");
	if (!in) {
		CON_Print("  fixes: can't open src: "); CON_Print(src); CON_Print("\n");
		return;
	}
	out = fopen(dst, "wb");
	if (!out) {
		fclose(in);
		CON_Print("  fixes: can't open dst: "); CON_Print(dst); CON_Print("\n");
		return;
	}
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		fwrite(buf, 1, n, out);
	fclose(in);
	fclose(out);
}

static void PS4_MakePath(const char *path)
{
	char tmp[256];
	char *p;
	Q_strncpyz(tmp, path, sizeof(tmp));
	for (p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			mkdir(tmp, 0755);
			*p = '/';
		}
	}
	mkdir(tmp, 0755);
}

/* Recursively copies src/ into dst/, mirroring the directory tree. */
static qboolean PS4_CopyDir(const char *src, const char *dst)
{
	DIR *d;
	struct dirent *ent;
	char srcpath[256], dstpath[256];
	struct stat st;
	qboolean ok = qtrue;

	d = opendir(src);
	if (!d) {
		CON_Print("  fixes: can't open dir: "); CON_Print(src); CON_Print("\n");
		return qfalse;
	}

	PS4_MakePath(dst);

	while ((ent = readdir(d)) != NULL) {
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
			continue;
		snprintf(srcpath, sizeof(srcpath), "%s/%s", src, ent->d_name);
		snprintf(dstpath, sizeof(dstpath), "%s/%s", dst, ent->d_name);
		if (stat(srcpath, &st) != 0)
			continue;
		if (S_ISDIR(st.st_mode)) {
			if (!PS4_CopyDir(srcpath, dstpath))
				ok = qfalse;
		} else {
			CON_Print("  fixes: "); CON_Print(srcpath); CON_Print(" -> "); CON_Print(dstpath); CON_Print("\n");
			PS4_CopyFile(srcpath, dstpath);
		}
	}
	closedir(d);
	return ok;
}

static void PS4_InstallFixes(void)
{
#if defined(STANDALONEOA)
	const char *marker = "/data/ioq3/fixes_installed_oa.txt";
#elif defined(STANDALONETA)
	const char *marker = "/data/ioq3/fixes_installed_ta.txt";
#else
	const char *marker = "/data/ioq3/fixes_installed_q3.txt";
#endif
	FILE *f;

	/* Already installed on a previous boot. */
	f = fopen(marker, "r");
	if (f) { fclose(f); return; }

	CON_Print("fixes: installing...\n");

	PS4_MakePath("/data/ioq3");

	qboolean ok = qtrue;
#if defined(STANDALONEOA)
	if (!PS4_CopyDir("/app0/fixes/baseoa",      "/data/ioq3/baseoa"))      ok = qfalse;
#elif defined(STANDALONETA)
	if (!PS4_CopyDir("/app0/fixes/baseq3",      "/data/ioq3/baseq3"))      ok = qfalse;
	if (!PS4_CopyDir("/app0/fixes/missionpack", "/data/ioq3/missionpack")) ok = qfalse;
#else
	if (!PS4_CopyDir("/app0/fixes/baseq3",      "/data/ioq3/baseq3"))      ok = qfalse;
#endif

	if (ok) {
		f = fopen(marker, "w");
		if (f) { fputs("1", f); fclose(f); }
		CON_Print("fixes: done.\n");
	} else {
		CON_Print("fixes: errors during install, will retry next boot.\n");
	}
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
	/* TA is a mod layered over baseq3, not a standalone game. Setting fs_game
	 * to missionpack here layers the TA paks on top of baseq3 paks. Never set
	 * BASEGAME as fs_game for TA — that cuts baseq3 out and causes !foundPak crash. */
	if (!strstr(commandLine, "+set fs_game"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set fs_game missionpack");
#endif

	/* Default player name = PSN username. Always set so OA/TA don't fall back
	 * to "UnnamedPlayer". The engine will override with the saved name from
	 * q3config.cfg after Com_Init, so returning players keep their custom name. */
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
	/* sv_pure 1 (default) blocks snapshot delivery until pak checksum auth
	 * completes. On PS4, cgame load takes several seconds; the server resends
	 * the gamestate on every usercmd before pureAuthentic is set (sv_client.c:1734),
	 * causing an infinite "Awaiting Snapshot" loop. Same fix as PS3 port. */
	if (!strstr(commandLine, "+set sv_pure"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set sv_pure 0");
	/* Suppress the warmup map_restart that Q3/TA qagame.qvm issues during
	 * G_InitGame. On PS4, cgame load takes several seconds; the server's
	 * sv.serverId advances during that time, the client sees its cp serverId
	 * as outdated, gamestate is resent, and the cycle repeats indefinitely
	 * (the "Awaiting Snapshot" loop). Disabling warmup means the server does
	 * one init and the client connects on the first try. */
	if (!strstr(commandLine, "+set g_doWarmup"))
		PS4_ADDARG(commandLine, sizeof(commandLine), "+set g_doWarmup 0");

	/* Interpreted VM only (VMI_BYTECODE=1). JIT (2) is not compiled in on PS4
	 * and causes cgame VM_Restart to fail during map load, hanging at CA_PRIMED. */
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

	PS4_InstallFixes();

	Com_Init(commandLine);
	PS4_ApplyDefaultBindings();
	NET_Init();

	while (1) {
		Com_Frame();
	}

	return 0;
}