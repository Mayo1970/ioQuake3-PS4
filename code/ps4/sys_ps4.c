/*
 * sys_ps4.c - PS4 system-level functions
 *
 * Replaces code/sys/sys_unix.c and sys_win32.c for the PS4 platform.
 * Provides filesystem, timing, paths, and other OS-level abstractions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <locale.h>

#include <orbis/libkernel.h>
#include <orbis/Rtc.h>
#include <orbis/UserService.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../sys/sys_local.h"

#define MAX_FOUND_FILES 0x1000

static uint64_t sys_timeBase = 0;

// Forward declaration
static void Sys_ListFilteredFiles(const char *basedir, char *subdirs,
	const char *filter, char **list, int *numfiles);

/*
 * Sys_PlatformInit
 */
void Sys_PlatformInit(void)
{
	struct timeval tv;
	setlocale(LC_ALL, "C"); // Ensure float parsing (e.g. j_pitch 0.022) works in all regions
	gettimeofday(&tv, NULL);
	sys_timeBase = tv.tv_sec;
}

/*
 * Sys_PlatformExit
 */
void Sys_PlatformExit(void)
{
}

/*
 * Sys_Milliseconds
 *
 * Returns milliseconds since engine start.
 */
int Sys_Milliseconds(void)
{
	struct timeval tp;
	gettimeofday(&tp, NULL);

	if (!sys_timeBase) {
		sys_timeBase = tp.tv_sec;
		return tp.tv_usec / 1000;
	}

	return (tp.tv_sec - sys_timeBase) * 1000 + tp.tv_usec / 1000;
}

/*
 * Sys_Sleep
 */
void Sys_Sleep(int msec)
{
	if (msec <= 0) return;
	sceKernelUsleep(msec * 1000);
}

/*
 * Sys_Cwd
 *
 * On PS4, the application directory is /app0/
 */
char *Sys_Cwd(void)
{
	static char cwd[MAX_OSPATH] = "/app0";
	return cwd;
}

/*
 * Sys_DefaultHomePath
 *
 * On PS4, writable save data goes to /savedata0/ (requires mounting).
 * For initial development, use /data/ or /temp0/ as scratch space.
 */
char *Sys_DefaultHomePath(void)
{
	static char path[] = "/data/ioq3";
	return path;
}

/*
 * Sys_Mkdir
 */
qboolean Sys_Mkdir(const char *path)
{
	int result = mkdir(path, 0777);
	if (result != 0 && errno != EEXIST) {
		return qfalse;
	}
	return qtrue;
}

/*
 * Sys_FOpen
 */
FILE *Sys_FOpen(const char *ospath, const char *mode)
{
	return fopen(ospath, mode);
}

/*
 * Sys_Pwd
 */
const char *Sys_Pwd(void)
{
	return Sys_Cwd();
}

/*
 * Sys_ListFiles
 *
 * Directory listing using POSIX opendir/readdir (supported on PS4 via musl).
 */
char **Sys_ListFiles(const char *directory, const char *extension,
	char *filter, int *numfiles, qboolean wantsubs)
{
	struct dirent *d;
	DIR *fdir;
	qboolean dironly = wantsubs;
	char search[MAX_OSPATH];
	int nfiles = 0;
	char **listCopy;
	char *list[MAX_FOUND_FILES];
	int extLen;
	int i;

	if (filter) {
		nfiles = 0;
		Sys_ListFilteredFiles(directory, "", filter, list, &nfiles);
		*numfiles = nfiles;
		if (!nfiles) return NULL;
		listCopy = Z_Malloc((nfiles + 1) * sizeof(*listCopy));
		for (i = 0; i < nfiles; i++) {
			listCopy[i] = list[i];
		}
		listCopy[i] = NULL;
		return listCopy;
	}

	if (!extension) extension = "";
	extLen = strlen(extension);

	fdir = opendir(directory);
	if (!fdir) {
		*numfiles = 0;
		return NULL;
	}

	while ((d = readdir(fdir)) != NULL) {
		if (nfiles >= MAX_FOUND_FILES - 1) break;

		Com_sprintf(search, sizeof(search), "%s/%s", directory, d->d_name);

		struct stat st;
		if (stat(search, &st) == -1) continue;

		if (dironly) {
			if (!S_ISDIR(st.st_mode)) continue;
		} else {
			if (S_ISDIR(st.st_mode)) continue;
		}

		if (*extension) {
			int nameLen = strlen(d->d_name);
			if (nameLen < extLen ||
				Q_stricmp(d->d_name + nameLen - extLen, extension)) {
				continue;
			}
		}

		list[nfiles] = CopyString(d->d_name);
		nfiles++;
	}

	closedir(fdir);

	*numfiles = nfiles;
	if (!nfiles) return NULL;

	listCopy = Z_Malloc((nfiles + 1) * sizeof(*listCopy));
	for (i = 0; i < nfiles; i++) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	return listCopy;
}

/*
 * Sys_FreeFileList
 */
void Sys_FreeFileList(char **list)
{
	int i;
	if (!list) return;
	for (i = 0; list[i]; i++) {
		Z_Free(list[i]);
	}
	Z_Free(list);
}

/*
 * Sys_RandomBytes
 */
qboolean Sys_RandomBytes(byte *string, int len)
{
	// PS4 has /dev/urandom equivalent via libkernel
	// For now, use a simple PRNG seeded from process time
	uint64_t seed = sceKernelGetProcessTime();
	int i;
	for (i = 0; i < len; i++) {
		seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
		string[i] = (byte)(seed >> 33);
	}
	return qtrue;
}

/*
 * Sys_GetCurrentUser
 */
char *Sys_GetCurrentUser(void)
{
	return "PS4 Player";
}

/*
 * Sys_Dialog - Show a message dialog on PS4
 * For now, just print to console.
 */
dialogResult_t Sys_Dialog(dialogType_t type, const char *message, const char *title)
{
	Com_Printf("[%s] %s\n", title ? title : "Dialog", message);
	return DR_OK;
}

// Sys_ErrorDialog is implemented in sys_main_ps4.c (uses MsgDialog)

/*
 * Sys_LowPhysicalMemory
 */
qboolean Sys_LowPhysicalMemory(void)
{
	return qfalse;  // PS4 has plenty of memory
}

/*
 * Sys_LoadDll / Sys_UnloadDll
 *
 * On PS4, dynamic module loading uses sceKernelLoadStartModule.
 * For initial development, game modules are statically linked.
 */
void *Sys_LoadDll(const char *name, qboolean useSystemLib)
{
	Com_Printf("Sys_LoadDll(%s): Not yet implemented on PS4\n", name);
	return NULL;
}

void Sys_UnloadDll(void *dllHandle)
{
}

void *Sys_LoadGameDll(const char *name,
	vmMainProc *entryPoint,
	intptr_t(QDECL *systemcalls)(intptr_t, ...))
{
	Com_Printf("Sys_LoadGameDll(%s): Not yet implemented on PS4\n", name);
	return NULL;
}

/*
 * Sys_SendPacket / Sys_GetPacket stubs
 * Actual networking is in net_ps4.c
 */

/*
 * Sys_DefaultInstallPath
 */
char *Sys_DefaultInstallPath(void)
{
	static char path[] = "/app0";
	return path;
}

/*
 * Sys_SetDefaultInstallPath
 */
void Sys_SetDefaultInstallPath(const char *path)
{
}

/*
 * Sys_TempPath
 */
char *Sys_TempPath(void)
{
	static char path[] = "/temp0";
	return path;
}

/*
 * Sys_DefaultAppPath
 */
char *Sys_DefaultAppPath(void)
{
	static char path[] = "/app0";
	return path;
}

/*
 * Sys_DefaultHomeConfigPath / Sys_DefaultHomeDataPath / Sys_DefaultHomeStatePath
 *
 * On PS4, all writable paths go to /data/ioq3.
 * These are called by files.c FS_Startup.
 */
char *Sys_DefaultHomeConfigPath(void)
{
	static char path[] = "/data/ioq3";
	return path;
}

char *Sys_DefaultHomeDataPath(void)
{
	static char path[] = "/data/ioq3";
	return path;
}

char *Sys_DefaultHomeStatePath(void)
{
	static char path[] = "/data/ioq3";
	return path;
}

/*
 * Sys_SteamPath / Sys_GogPath / Sys_MicrosoftStorePath
 *
 * Not applicable on PS4. Return empty string.
 */
char *Sys_SteamPath(void)
{
	static char path[] = "";
	return path;
}

char *Sys_GogPath(void)
{
	static char path[] = "";
	return path;
}

char *Sys_MicrosoftStorePath(void)
{
	static char path[] = "";
	return path;
}

/*
 * Sys_BinaryPath
 *
 * Returns the path to the running executable.
 * On PS4, this is /app0 (the read-only app install directory).
 */
char *Sys_BinaryPath(void)
{
	static char path[MAX_OSPATH] = "/app0";
	return path;
}

/*
 * Sys_SetBinaryPath - No-op on PS4 (always /app0)
 */
void Sys_SetBinaryPath(const char *path)
{
	(void)path;
}

/*
 * Sys_SigHandler
 */
void Sys_SigHandler(int signal)
{
	Com_Printf("Received signal %d, shutting down...\n", signal);
	Sys_Quit();
}

/*
 * Sys_PIDIsRunning - Not meaningful on PS4
 */
qboolean Sys_PIDIsRunning(int pid)
{
	return qfalse;
}

/*
 * Sys_PID - Return a fixed PID since PS4 doesn't expose PIDs normally
 */
int Sys_PID(void)
{
	return 1;
}

/*
 * Sys_OpenFolderInPlatformFileManager - Not applicable on PS4
 */
qboolean Sys_OpenFolderInPlatformFileManager(const char *path)
{
	return qfalse;
}

/*
 * Sys_SetMaxFileLimit - No-op on PS4
 */
qboolean Sys_SetMaxFileLimit(void)
{
	return qtrue;
}

/*
 * Sys_GLimpSafeInit / Sys_GLimpInit - No-op on PS4
 * The PS4 GL init is handled entirely by ps4_glimp.c
 */
void Sys_GLimpSafeInit(void)
{
}

void Sys_GLimpInit(void)
{
}

/*
 * Sys_ListFilteredFiles
 *
 * Recursive directory listing with filter matching.
 * Required by Sys_ListFiles when a filter is provided.
 */
static void Sys_ListFilteredFiles(const char *basedir, char *subdirs,
	const char *filter, char **list, int *numfiles)
{
	char search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
	char filename[MAX_OSPATH];
	DIR *fdir;
	struct dirent *d;
	struct stat st;

	if (*numfiles >= MAX_FOUND_FILES - 1)
		return;

	if (basedir[0] == '\0')
		return;

	if (strlen(subdirs))
		Com_sprintf(search, sizeof(search), "%s/%s", basedir, subdirs);
	else
		Com_sprintf(search, sizeof(search), "%s", basedir);

	if ((fdir = opendir(search)) == NULL)
		return;

	while ((d = readdir(fdir)) != NULL) {
		Com_sprintf(filename, sizeof(filename), "%s/%s", search, d->d_name);
		if (stat(filename, &st) == -1)
			continue;

		if (S_ISDIR(st.st_mode)) {
			if (Q_stricmp(d->d_name, ".") && Q_stricmp(d->d_name, "..")) {
				if (strlen(subdirs))
					Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s/%s", subdirs, d->d_name);
				else
					Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s", d->d_name);
				Sys_ListFilteredFiles(basedir, newsubdirs, filter, list, numfiles);
			}
		}
		if (*numfiles >= MAX_FOUND_FILES - 1)
			break;

		Com_sprintf(filename, sizeof(filename), "%s/%s", subdirs, d->d_name);
		if (!Com_FilterPath(filter, filename, qfalse))
			continue;

		list[*numfiles] = CopyString(filename);
		(*numfiles)++;
	}

	closedir(fdir);
}

/*
 * Sys_DllExtension
 *
 * Check if filename has the platform dynamic library extension.
 * On PS4, this is .prx (though we don't actually load DLLs).
 */
qboolean Sys_DllExtension(const char *name)
{
	const char *p;

	if ((p = strrchr(name, '.')) != NULL) {
		if (!Q_stricmp(p, ".prx"))
			return qtrue;
	}
	return qfalse;
}

/*
 * Sys_Mkfifo - FIFOs not supported on PS4
 */
FILE *Sys_Mkfifo(const char *ospath)
{
	return NULL;
}

/*
 * Sys_InitPIDFile / Sys_RemovePIDFile
 *
 * PID file management is meaningless on PS4 (single-instance console app).
 */
void Sys_InitPIDFile(const char *gamedir)
{
}

void Sys_RemovePIDFile(const char *gamedir)
{
}

/*
 * Sys_OpenFolderInFileManager - Not applicable on PS4
 */
qboolean Sys_OpenFolderInFileManager(const char *path, qboolean create)
{
	return qfalse;
}

/*
 * Sys_ConsoleInput - No interactive console on PS4
 */
char *Sys_ConsoleInput(void)
{
	return NULL;
}

/*
 * Sys_In_Restart_f
 */
void Sys_In_Restart_f(void)
{
	IN_Restart();
}

/*
 * Sys_AnsiColorPrint - PS4 stdout doesn't support ANSI colors
 */
void Sys_AnsiColorPrint(const char *msg)
{
	fputs(msg, stdout);
}

/*
 * Sys_FileTime
 */
int Sys_FileTime(char *path)
{
	struct stat buf;

	if (stat(path, &buf) == -1)
		return -1;

	return buf.st_mtime;
}

/*
 * Sys_GetProcessorFeatures
 */
cpuFeatures_t Sys_GetProcessorFeatures(void)
{
	// PS4 Jaguar CPU: x86_64 with SSE2
	return CF_RDTSC | CF_MMX | CF_SSE | CF_SSE2;
}

/*
 * Sys_ParseArgs - No command-line parsing on PS4
 */
void Sys_ParseArgs(int argc, char **argv)
{
}

/*
 * Sys_SetEnv - Set/unset environment variable
 *
 * PS4 has no user-accessible environment, but setenv is available
 * via the POSIX compatibility layer for internal engine use.
 */
void Sys_SetEnv(const char *name, const char *value)
{
	if (value && *value)
		setenv(name, value, 1);
	else
		unsetenv(name);
}

/*
 * Sys_GetClipboardData - No clipboard on PS4
 */
char *Sys_GetClipboardData(void)
{
	return NULL;
}
