/* sys_ps4.c -- PS4 OS layer (paths, timing, filesystem). */

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

static void Sys_ListFilteredFiles(const char *basedir, char *subdirs,
	const char *filter, char **list, int *numfiles);

void Sys_PlatformInit(void)
{
	struct timeval tv;
	setlocale(LC_ALL, "C"); /* locale-agnostic float parsing (j_pitch etc.) */
	gettimeofday(&tv, NULL);
	sys_timeBase = tv.tv_sec;
}

void Sys_PlatformExit(void)
{
}

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

void Sys_Sleep(int msec)
{
	if (msec <= 0) return;
	sceKernelUsleep(msec * 1000);
}

char *Sys_Cwd(void)
{
	static char cwd[MAX_OSPATH] = "/app0";
	return cwd;
}

char *Sys_DefaultHomePath(void)
{
	static char path[] = "/data/ioq3";
	return path;
}

qboolean Sys_Mkdir(const char *path)
{
	int result = mkdir(path, 0777);
	if (result != 0 && errno != EEXIST) {
		return qfalse;
	}
	return qtrue;
}

FILE *Sys_FOpen(const char *ospath, const char *mode)
{
	return fopen(ospath, mode);
}

const char *Sys_Pwd(void)
{
	return Sys_Cwd();
}

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

void Sys_FreeFileList(char **list)
{
	int i;
	if (!list) return;
	for (i = 0; list[i]; i++) {
		Z_Free(list[i]);
	}
	Z_Free(list);
}

/* PRNG seeded from process time; not cryptographically secure. */
qboolean Sys_RandomBytes(byte *string, int len)
{
	uint64_t seed = sceKernelGetProcessTime();
	int i;
	for (i = 0; i < len; i++) {
		seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
		string[i] = (byte)(seed >> 33);
	}
	return qtrue;
}

char *Sys_GetCurrentUser(void)
{
	return "PS4 Player";
}

/* Stub: real MsgDialog lives in sys_main_ps4.c::Sys_ErrorDialog. */
dialogResult_t Sys_Dialog(dialogType_t type, const char *message, const char *title)
{
	Com_Printf("[%s] %s\n", title ? title : "Dialog", message);
	return DR_OK;
}

qboolean Sys_LowPhysicalMemory(void)
{
	return qfalse;
}

/* Game modules are statically linked on PS4; no runtime DLL loading. */
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

char *Sys_DefaultInstallPath(void)
{
	static char path[] = "/app0";
	return path;
}

void Sys_SetDefaultInstallPath(const char *path)
{
}

char *Sys_TempPath(void)
{
	static char path[] = "/temp0";
	return path;
}

char *Sys_DefaultAppPath(void)
{
	static char path[] = "/app0";
	return path;
}

/* All writable PS4 paths go to /data/ioq3. */
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

/* No PC storefronts on PS4. */
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

char *Sys_BinaryPath(void)
{
	static char path[MAX_OSPATH] = "/app0";
	return path;
}

void Sys_SetBinaryPath(const char *path)
{
	(void)path;
}

void Sys_SigHandler(int signal)
{
	Com_Printf("Received signal %d, shutting down...\n", signal);
	Sys_Quit();
}

qboolean Sys_PIDIsRunning(int pid)
{
	return qfalse;
}

int Sys_PID(void)
{
	return 1;
}

qboolean Sys_OpenFolderInPlatformFileManager(const char *path)
{
	return qfalse;
}

qboolean Sys_SetMaxFileLimit(void)
{
	return qtrue;
}

/* GL init is in ps4_glimp.c. */
void Sys_GLimpSafeInit(void) { }
void Sys_GLimpInit(void) { }

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

qboolean Sys_DllExtension(const char *name)
{
	const char *p;

	if ((p = strrchr(name, '.')) != NULL) {
		if (!Q_stricmp(p, ".prx"))
			return qtrue;
	}
	return qfalse;
}

FILE *Sys_Mkfifo(const char *ospath)
{
	return NULL;
}

void Sys_InitPIDFile(const char *gamedir) { }
void Sys_RemovePIDFile(const char *gamedir) { }

qboolean Sys_OpenFolderInFileManager(const char *path, qboolean create)
{
	return qfalse;
}

char *Sys_ConsoleInput(void)
{
	return NULL;
}

void Sys_In_Restart_f(void)
{
	IN_Restart();
}

void Sys_AnsiColorPrint(const char *msg)
{
	fputs(msg, stdout);
}

int Sys_FileTime(char *path)
{
	struct stat buf;

	if (stat(path, &buf) == -1)
		return -1;

	return buf.st_mtime;
}

/* PS4 Jaguar CPU: x86_64 with SSE2. */
cpuFeatures_t Sys_GetProcessorFeatures(void)
{
	return CF_RDTSC | CF_MMX | CF_SSE | CF_SSE2;
}

void Sys_ParseArgs(int argc, char **argv) { }

void Sys_SetEnv(const char *name, const char *value)
{
	if (value && *value)
		setenv(name, value, 1);
	else
		unsetenv(name);
}

char *Sys_GetClipboardData(void)
{
	return NULL;
}
