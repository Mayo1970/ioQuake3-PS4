/*
 * con_ps4.c - PS4 console backend (passive, debug output)
 *
 * Replaces code/sys/con_tty.c and con_win32.c.
 * On PS4 there is no interactive terminal.
 * All output is written to /data/ioq3/ioq3_log.txt for post-mortem debugging.
 * Based on code/sys/con_passive.c from ioQuake3.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <orbis/libkernel.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../sys/sys_local.h"

#define PS4_LOG_PATH "/data/ioq3/ioquake3log.txt"

static FILE *s_logFile = NULL;

/*
 * CON_Init
 */
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

/*
 * CON_Input - No interactive input on PS4
 */
char *CON_Input(void)
{
	return NULL;
}

/*
 * CON_Print
 */
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

/*
 * CON_Shutdown
 */
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

/*
 * CON_LogSize / CON_LogWrite / CON_LogRead
 *
 * Console log buffer for dedicated server console. Not used on PS4 but
 * declared in sys_local.h, so stubs are provided for link safety.
 */
unsigned int CON_LogSize(void)
{
	return 0;
}

unsigned int CON_LogWrite(const char *in)
{
	return 0;
}

unsigned int CON_LogRead(char *out, unsigned int outSize)
{
	return 0;
}
