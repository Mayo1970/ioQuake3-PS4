/*
 * code/ps4/ps4_account.h -- PSN username lookup for the default player name.
 *
 * This has to feed Cvar_Get's *default* (the cvar's resetString), not a
 * "+set name" on the command line: a command-line set is only a value, and the
 * setup menu's Defaults button (exec default.cfg + cvar_restart) restores every
 * cvar to its resetString -- which would be the stock "UnnamedPlayer".
 */
#ifndef PS4_ACCOUNT_H
#define PS4_ACCOUNT_H

/*
 * Caches the initial user's PSN name. Call once early in boot (before Com_Init)
 * so the sceUserService hit stays at the timing already proven on hardware;
 * PS4_DefaultPlayerName() is then a pure read, safe to call from CL_Init.
 */
void PS4_InitDefaultPlayerName(void);

/* Cached PSN username, or "UnnamedPlayer" if unavailable. Never NULL. */
const char *PS4_DefaultPlayerName(void);

#endif /* PS4_ACCOUNT_H */
