/* con_ps4.c -- passive console backend. Debug builds log to a file. */

#include <stdio.h>
#include <sys/stat.h>
#include <orbis/libkernel.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../sys/sys_local.h"

#define PS4_LOG_PATH "/data/ioq3/ioquake3log.txt"

static FILE *s_logFile = NULL;

void CON_Init(void)
{
#ifdef PS4_DEBUG
	mkdir("/data/ioq3", 0777);
	s_logFile = fopen(PS4_LOG_PATH, "w");
	if (s_logFile) {
		fprintf(s_logFile, "=== ioQuake3 PS4 Log ===\n");
		fflush(s_logFile);
	}
#endif
}

char *CON_Input(void)
{
	return NULL;
}

void CON_Print(const char *msg)
{
#ifdef PS4_DEBUG
	if (msg && s_logFile) {
		fputs(msg, s_logFile);
		fflush(s_logFile);
	}
#else
	(void)msg;
#endif
}

void CON_Shutdown(void)
{
#ifdef PS4_DEBUG
	if (s_logFile) {
		fprintf(s_logFile, "=== Shutdown ===\n");
		fclose(s_logFile);
		s_logFile = NULL;
	}
#endif
}

/* Dedicated-server log buffer; unused on PS4, stubs for link safety. */
unsigned int CON_LogSize(void)                              { return 0; }
unsigned int CON_LogWrite(const char *in)                   { return 0; }
unsigned int CON_LogRead(char *out, unsigned int outSize)   { return 0; }
