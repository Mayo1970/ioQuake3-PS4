/* ps4_input.c -- DualShock 4 input via scePad. */

#include <string.h>
#include <wchar.h>
#include <math.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/ImeDialog.h>
#include <orbis/libkernel.h>

#include "../client/client.h"
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

extern int   Key_GetCatcher(void);
extern void  Key_SetBinding(int keynum, const char *binding);
extern char *Key_GetBinding(int keynum);

extern void Con_PageUp(void);
extern void Con_PageDown(void);
extern void Con_Top(void);
extern void Con_Bottom(void);

extern void PS4_AudioPause(void);
extern void PS4_AudioResume(void);

/* Always set, overriding any config binding, so every button has a default. */
static void PS4_SetDefaultBind(int keynum, const char *keyname, const char *binding)
{
    Key_SetBinding(keynum, binding);
}

/* Apply all PS4 button bindings. Called after Com_Init so config is fully loaded.
   Also unbinds conflicting keyboard bindings so PS4 buttons take precedence in the UI. */
void PS4_ApplyDefaultBindings(void)
{
    PS4_SetDefaultBind(K_JOY1, "JOY1", "+moveup");      /* Cross    = jump        */
    PS4_SetDefaultBind(K_JOY2, "JOY2", "+movedown");    /* Circle   = crouch      */
    PS4_SetDefaultBind(K_JOY3, "JOY3", "weapprev");     /* Square   = prev weapon */
    PS4_SetDefaultBind(K_JOY4, "JOY4", "weapnext");     /* Triangle = next weapon */
    PS4_SetDefaultBind(K_JOY5, "JOY5", "+moveleft");    /* L1       = strafe left */
    PS4_SetDefaultBind(K_JOY6, "JOY6", "+button2");     /* R1       = use item    */
    PS4_SetDefaultBind(K_JOY7, "JOY7", "+zoom");        /* L2       = zoom        */
    PS4_SetDefaultBind(K_JOY8, "JOY8", "+attack");      /* R2       = fire        */
    PS4_SetDefaultBind(K_JOY9, "JOY9", "+speed");       /* L3       = walk toggle */
    PS4_SetDefaultBind(K_JOY11,"JOY11", "+scores");     /* Touchpad = scoreboard  */
    PS4_SetDefaultBind(K_ESCAPE,"ESCAPE", "togglemenu");/* Options  = toggle menu */

    /* Left stick cosmetic bindings (analog axis drives movement,
       these are display-only so the Controls menu shows LS_UP etc.) */
    PS4_SetDefaultBind(K_UPARROW,    "UPARROW",    "+forward");
    PS4_SetDefaultBind(K_DOWNARROW,  "DOWNARROW",  "+back");
    PS4_SetDefaultBind(K_LEFTARROW,  "LEFTARROW",  "+moveleft");
    PS4_SetDefaultBind(K_RIGHTARROW, "RIGHTARROW", "+moveright");

    /* Right stick cosmetic bindings */
    PS4_SetDefaultBind(K_PGDN, "PGDN", "+lookup");     /* RS_UP   = look up   */
    PS4_SetDefaultBind(K_DEL,  "DEL",  "+lookdown");   /* RS_DOWN = look down */

    /* Unbind legacy keyboard keys that conflict with PS4 buttons
       so the Controls menu prefers button/stick displays. */
    Key_SetBinding('w', "");                            /* walk forward was on W */
    Key_SetBinding('s', "");                            /* backpedal was on S */
    Key_SetBinding('a', "");                            /* step left was on A */
    Key_SetBinding('d', "");                            /* step right was on D */
    Key_SetBinding('[', "");                            /* prev weapon was on [ */
    Key_SetBinding(']', "");                            /* next weapon was on ] */
    Key_SetBinding('/', "");                            /* was bound to weapnext */
    Key_SetBinding(K_CTRL, "");                         /* attack was on CTRL */
    Key_SetBinding(K_ENTER, "");                        /* use item was on ENTER */
    Key_SetBinding(K_SHIFT, "");                        /* run/walk was on SHIFT */
    Key_SetBinding(K_END, "");                          /* center view was on END */
    Key_SetBinding(K_ALT, "");                          /* sidestep was on ALT */
}

#define STICK_CENTER    128
#define STICK_DEADZONE  30
#define STICK_RANGE     (128 - STICK_DEADZONE)

#define MENU_CURSOR_SPEED 5.0f

#define TRIGGER_THRESHOLD 30    /* analog L2/R2 threshold (0-255) */

#define NUM_DS4_BUTTONS 16      /* 14 digital + 2 analog triggers */


#define BTN_IDX_CROSS      0
#define BTN_IDX_CIRCLE     1
#define BTN_IDX_SQUARE     2
#define BTN_IDX_TRIANGLE   3
#define BTN_IDX_L1         4
#define BTN_IDX_R1         5
#define BTN_IDX_L3         6
#define BTN_IDX_R3         7
#define BTN_IDX_OPTIONS    8
#define BTN_IDX_TOUCHPAD   9
#define BTN_IDX_DPAD_UP    10
#define BTN_IDX_DPAD_DOWN  11
#define BTN_IDX_DPAD_LEFT  12
#define BTN_IDX_DPAD_RIGHT 13
#define BTN_IDX_L2_ANALOG  14   /* L2 via analogButtons.l2 threshold */
#define BTN_IDX_R2_ANALOG  15   /* R2 via analogButtons.r2 threshold */

/* Q3 key mapped to each button index */
static const int ds4_key_map[NUM_DS4_BUTTONS] = {
    K_JOY1,       /*  0: Cross    -> jump / menu accept  */
    K_JOY2,       /*  1: Circle   -> crouch / menu accept */
    K_JOY3,       /*  2: Square   -> prev weapon / menu back */
    K_JOY4,       /*  3: Triangle -> next weapon / menu back */
    K_JOY5,       /*  4: L1       -> strafe left   */
    K_JOY6,       /*  5: R1       -> strafe right  */
    K_JOY9,       /*  6: L3       -> walk toggle   */
    K_JOY10,      /*  7: R3       -> scoreboard    */
    K_ESCAPE,     /*  8: Options  -> menu / escape */
    K_JOY11,      /*  9: Touchpad -> scoreboard    */
    K_UPARROW,    /* 10: D-Up                      */
    K_DOWNARROW,  /* 11: D-Down                    */
    K_LEFTARROW,  /* 12: D-Left                    */
    K_RIGHTARROW, /* 13: D-Right                   */
    K_JOY7,       /* 14: L2 analog -> zoom         */
    K_JOY8,       /* 15: R2 analog -> fire         */
};

static int          s_padHandle  = -1;
static int          s_userId     = -1;

/* Oversize: FW 9.00 scePadReadState writes past the declared struct end. */
static union {
    OrbisPadData data;
    uint8_t pad[256];
} s_padDataBuf;
#define s_padData (s_padDataBuf.data)

static int   ds4_btn_prev[NUM_DS4_BUTTONS];
static int   ds4_axis_prev[4];           /* last sent LX LY RX RY */
static float ds4_cursor_x = 0.0f;
static float ds4_cursor_y = 0.0f;

/* Aim mode: 0=stick (default), 1=touchpad. s_touchPrevX/Y=-1 means no contact. */
static int   s_aimMode    = 0;
static float s_aimAccumX  = 0.0f;
static float s_aimAccumY  = 0.0f;
static float s_touchPrevX = -1.0f;
static float s_touchPrevY = -1.0f;

static cvar_t *ps4_aimSensX  = NULL;
static cvar_t *ps4_aimSensY  = NULL;
static cvar_t *ps4_rumbleEnable   = NULL;
static cvar_t *ps4_rumbleScale    = NULL;

/* Active rumble pulse: motors run until s_rumbleExpiryMs, then zeroed. */
static int s_rumbleExpiryMs = 0;
static int s_rumbleActive   = 0;

/* Flag: OSK dialog is currently open. Prevents pad events from leaking
 * into the engine while the system IME has focus. */
static int s_oskActive = 0;

/* Queue a rumble pulse. Overlapping pulses: strongest wins, no summing. */
void PS4_SetRumble(uint8_t large, uint8_t small, int durationMs)
{
    if (s_padHandle < 0) return;
    if (!ps4_rumbleEnable || !ps4_rumbleEnable->integer) return;

    float scale = ps4_rumbleScale ? ps4_rumbleScale->value : 1.0f;
    if (scale < 0.0f) scale = 0.0f;
    if (scale > 1.0f) scale = 1.0f;

    int lg = (int)(large * scale);
    int sm = (int)(small * scale);
    if (lg > 255) lg = 255;
    if (sm > 255) sm = 255;

    OrbisPadVibeParam v;
    v.lgMotor = (uint8_t)lg;
    v.smMotor = (uint8_t)sm;
    scePadSetVibration(s_padHandle, &v);

    int now    = Sys_Milliseconds();
    int expiry = now + durationMs;
    if (expiry > s_rumbleExpiryMs) s_rumbleExpiryMs = expiry;
    s_rumbleActive = 1;
}

/* Stop motors once the pulse expires. */
static void PS4_RumbleTick(void)
{
    if (!s_rumbleActive) return;
    if (Sys_Milliseconds() < s_rumbleExpiryMs) return;

    OrbisPadVibeParam v = { 0, 0 };
    scePadSetVibration(s_padHandle, &v);
    s_rumbleActive   = 0;
    s_rumbleExpiryMs = 0;
}

static int s_frameCount  = 0;
static int s_initDone    = 0;

/* Health lightbar state. Updated every frame from cl.snap.ps.stats[STAT_HEALTH].
 * Three states:
 *   0 = normal (aim-mode color: blue or cyan)
 *   1 = low health 26-50: static dim red
 *   2 = critical health 1-25: pulsing bright red
 *   3 = dead (health <= 0): solid dark red, no pulse
 * Only active when clc.state == CA_ACTIVE and snap is valid. */
static int s_healthLightbarState = 0;  /* 0=normal,1=low,2=critical,3=dead */
static int s_healthLightbarLastMs = 0; /* time of last lightbar write */

/* Write the aim-mode lightbar color (blue=stick, cyan=touchpad).
 * Called on init, on aim-mode toggle, and when returning to normal health. */
static void PS4_SetAimModeColor(void)
{
    if (s_padHandle < 0) return;
    if (s_aimMode == 1) {
        OrbisPadColor col = { 0, 255, 255, 0 }; /* cyan = touchpad */
        scePadSetLightBar(s_padHandle, &col);
    } else {
        OrbisPadColor col = { 0, 0, 255, 0 };   /* blue = stick */
        scePadSetLightBar(s_padHandle, &col);
    }
}

/* Called once per IN_Frame. Reads health from the client snapshot and updates
 * the lightbar. scePadSetLightBar is only called when state changes or during
 * the pulse animation (≤25 HP), so the call rate is low in the normal case. */
static void PS4_UpdateHealthLightbar(void)
{
    extern clientActive_t cl;
    extern clientConnection_t clc;

    if (s_padHandle < 0) return;

    /* Out of game: restore aim-mode color and reset. */
    if (clc.state != CA_ACTIVE || (cl.snap.snapFlags & SNAPFLAG_NOT_ACTIVE)) {
        if (s_healthLightbarState != 0) {
            s_healthLightbarState = 0;
            PS4_SetAimModeColor();
        }
        return;
    }

    int health = cl.snap.ps.stats[STAT_HEALTH];
    int now    = Sys_Milliseconds();

    if (health <= 0) {
        /* Dead: solid dark red, update only on state change. */
        if (s_healthLightbarState != 3) {
            s_healthLightbarState = 3;
            OrbisPadColor col = { 60, 0, 0, 0 };
            scePadSetLightBar(s_padHandle, &col);
        }
    } else if (health <= 25) {
        /* Critical: pulsing red. Sine wave with ~1s period.
         * Update every 33ms (30fps) to keep call rate reasonable. */
        s_healthLightbarState = 2;
        if (now - s_healthLightbarLastMs >= 33) {
            s_healthLightbarLastMs = now;
            /* sin() oscillates -1..1; map to 80..255 so it never goes fully off. */
            float phase = (float)(now % 1000) / 1000.0f; /* 0..1 per second */
            float s = sinf(phase * 6.2831853f);           /* full sine cycle */
            int r = (int)(167.5f + s * 87.5f);            /* 80..255 */
            OrbisPadColor col = { (uint8_t)r, 0, 0, 0 };
            scePadSetLightBar(s_padHandle, &col);
        }
    } else if (health <= 50) {
        /* Low: static dim red, update only on state change. */
        if (s_healthLightbarState != 1) {
            s_healthLightbarState = 1;
            OrbisPadColor col = { 120, 0, 0, 0 };
            scePadSetLightBar(s_padHandle, &col);
        }
    } else {
        /* Healthy: restore aim-mode color on state change. */
        if (s_healthLightbarState != 0) {
            s_healthLightbarState = 0;
            PS4_SetAimModeColor();
        }
    }
}

/* Decode pad button bitmask and analog triggers into the flat indexed array. */
static void DS4_ReadButtons(const OrbisPadData *pd, int out[NUM_DS4_BUTTONS])
{
    uint32_t b = pd->buttons;

    out[BTN_IDX_CROSS]      = (b & ORBIS_PAD_BUTTON_CROSS)     ? 1 : 0;
    out[BTN_IDX_CIRCLE]     = (b & ORBIS_PAD_BUTTON_CIRCLE)    ? 1 : 0;
    out[BTN_IDX_SQUARE]     = (b & ORBIS_PAD_BUTTON_SQUARE)    ? 1 : 0;
    out[BTN_IDX_TRIANGLE]   = (b & ORBIS_PAD_BUTTON_TRIANGLE)  ? 1 : 0;
    out[BTN_IDX_L1]         = (b & ORBIS_PAD_BUTTON_L1)        ? 1 : 0;
    out[BTN_IDX_R1]         = (b & ORBIS_PAD_BUTTON_R1)        ? 1 : 0;
    out[BTN_IDX_L3]         = (b & ORBIS_PAD_BUTTON_L3)        ? 1 : 0;
    out[BTN_IDX_R3]         = (b & ORBIS_PAD_BUTTON_R3)        ? 1 : 0;
    out[BTN_IDX_OPTIONS]    = (b & ORBIS_PAD_BUTTON_OPTIONS)   ? 1 : 0;
    out[BTN_IDX_TOUCHPAD]   = (b & ORBIS_PAD_BUTTON_TOUCH_PAD) ? 1 : 0;
    out[BTN_IDX_DPAD_UP]    = (b & ORBIS_PAD_BUTTON_UP)        ? 1 : 0;
    out[BTN_IDX_DPAD_DOWN]  = (b & ORBIS_PAD_BUTTON_DOWN)      ? 1 : 0;
    out[BTN_IDX_DPAD_LEFT]  = (b & ORBIS_PAD_BUTTON_LEFT)      ? 1 : 0;
    out[BTN_IDX_DPAD_RIGHT] = (b & ORBIS_PAD_BUTTON_RIGHT)     ? 1 : 0;

    /* Trigger pressed = digital bit OR analog pressure past threshold. */
    out[BTN_IDX_L2_ANALOG] = ((b & ORBIS_PAD_BUTTON_L2) ||
                               pd->analogButtons.l2 > TRIGGER_THRESHOLD) ? 1 : 0;
    out[BTN_IDX_R2_ANALOG] = ((b & ORBIS_PAD_BUTTON_R2) ||
                               pd->analogButtons.r2 > TRIGGER_THRESHOLD) ? 1 : 0;
}

/* Zero out all accumulated input state in the engine to prevent
 * phantom camera movement / stuck keys after OSK closes. */
static void PS4_FlushInputState(void)
{
    extern clientActive_t cl;
    
    /* Zero mouse accumulation - CL_MouseMove() reads these */
    cl.mouseDx[0] = 0;
    cl.mouseDx[1] = 0;
    cl.mouseDy[0] = 0;
    cl.mouseDy[1] = 0;
    
    memset(cl.joystickAxis, 0, sizeof(cl.joystickAxis));
}

/* Send key-up events for all buttons to unstick any held commands.
 * This prevents +attack, +zoom, etc. from staying active after OSK closes. */
static void PS4_UnstickAllButtons(void)
{
    int i;
    for (i = 0; i < NUM_DS4_BUTTONS; i++) {
        if (ds4_btn_prev[i]) {
            Com_QueueEvent(0, SE_KEY, ds4_key_map[i], qfalse, 0, NULL);
            ds4_btn_prev[i] = 0;
        }
    }
}

/* Show the system IME keyboard; returns qtrue if confirmed with non-empty text. */
static qboolean PS4_ShowOSK(const wchar_t *title, int maxLen, wchar_t *buf, int bufLen) {
    memset(buf, 0, bufLen * sizeof(wchar_t));

    OrbisImeDialogSetting setting;
    memset(&setting, 0, sizeof(setting));
    setting.userId              = s_userId;
    setting.type                = ORBIS_TYPE_BASIC_LATIN;
    setting.enterLabel          = ORBIS_BUTTON_LABEL_DEFAULT;
    setting.option              = 0;
    setting.maxTextLength       = maxLen;
    setting.inputTextBuffer     = buf;
    setting.posx                = 960.0f;
    setting.posy                = 540.0f;
    setting.horizontalAlignment = ORBIS_H_CENTER;
    setting.verticalAlignment   = ORBIS_V_CENTER;
    setting.title               = title;

    /* Pause audio: the IME suspends the game but the audio thread keeps
       looping stale DMA buffer data otherwise. */
    PS4_AudioPause();

    int ret = sceImeDialogInit(&setting, NULL);
    if (ret < 0) {
        Com_Printf("PS4 OSK: sceImeDialogInit failed: 0x%08X\n", (unsigned)ret);
        PS4_AudioResume();
        return qfalse;
    }

    s_oskActive = 1;

    OrbisDialogStatus status;
    do {
        status = sceImeDialogGetStatus();
        sceKernelUsleep(16000);
    } while (status == ORBIS_DIALOG_STATUS_RUNNING);

    qboolean confirmed = qfalse;
    if (status == ORBIS_DIALOG_STATUS_STOPPED) {
        OrbisDialogResult result;
        memset(&result, 0, sizeof(result));
        sceImeDialogGetResult(&result);
        confirmed = (result.endstatus == ORBIS_DIALOG_OK && buf[0]) ? qtrue : qfalse;
    }

    sceImeDialogTerm();
    s_oskActive = 0;

    PS4_AudioResume();
    PS4_UnstickAllButtons();
    PS4_FlushInputState();

    /* Re-anchor local pad state so held input doesn't replay after OSK. */
    scePadReadState(s_padHandle, &s_padDataBuf.data);
    memset(&s_padData, 0, sizeof(s_padData));
    memset(ds4_axis_prev, 0, sizeof(ds4_axis_prev));

    return confirmed;
}

/* Convert OSK wchar output to ASCII; non-ASCII becomes '?'. */
static void PS4_WcharToASCII(const wchar_t *src, char *dst, int dstLen)
{
    int i;
    for (i = 0; i < dstLen - 1 && src[i]; i++)
        dst[i] = (src[i] < 0x80) ? (char)src[i] : '?';
    dst[i] = '\0';
}

/* L1+Touchpad: prompt for a console command and execute it. */
static void PS4_ShowConsoleOSK(void)
{
    static wchar_t s_inputBuf[256];
    static const wchar_t s_title[] = L"Console Command";

    if (!PS4_ShowOSK(s_title, 255, s_inputBuf, 256))
        return;

    char cmd[256];
    PS4_WcharToASCII(s_inputBuf, cmd, sizeof(cmd));
    Com_Printf("PS4 Console: %s\n", cmd);
    Cbuf_AddText(va("%s\n", cmd));
}

/* Cross in menu with active ui_ime_target: open OSK and write result to ui_ime_text/done. */
static qboolean PS4_ShowFieldOSK(void)
{
    char target[64];
    Cvar_VariableStringBuffer("ui_ime_target", target, sizeof(target));
    if (!target[0])
        return qfalse;

    /* Pick a localised title; fall back to the raw field name for any unknown field. */
    wchar_t title[64];
    if      (strcmp(target, "domain")    == 0) { const wchar_t t[] = L"Server Address"; memcpy(title, t, sizeof(t)); }
    else if (strcmp(target, "port")      == 0) { const wchar_t t[] = L"Port";           memcpy(title, t, sizeof(t)); }
    else if (strcmp(target, "name")      == 0) { const wchar_t t[] = L"Player Name";    memcpy(title, t, sizeof(t)); }
    else if (strcmp(target, "hostname")  == 0) { const wchar_t t[] = L"Server Name";    memcpy(title, t, sizeof(t)); }
    else if (strcmp(target, "fraglimit") == 0) { const wchar_t t[] = L"Frag Limit";     memcpy(title, t, sizeof(t)); }
    else if (strcmp(target, "flaglimit") == 0) { const wchar_t t[] = L"Capture Limit";     memcpy(title, t, sizeof(t)); }
    else if (strcmp(target, "timelimit")    == 0) { const wchar_t t[] = L"Time Limit";     memcpy(title, t, sizeof(t)); }
    else if (strcmp(target, "capturelimit") == 0) { const wchar_t t[] = L"Enter Capture Limit"; memcpy(title, t, sizeof(t)); }
    else if (strcmp(target, "cdkey")        == 0) { const wchar_t t[] = L"CD Key";         memcpy(title, t, sizeof(t)); }
    else if (strcmp(target, "savename")  == 0) { const wchar_t t[] = L"Config Name";    memcpy(title, t, sizeof(t)); }
    else {
        /* Unknown field — convert ASCII name to wchar and use as title. */
        int i;
        for (i = 0; i < 63 && target[i]; i++) title[i] = (wchar_t)target[i];
        title[i] = L'\0';
    }

    /* Clear the target now — the draw function may not run again if we're navigating away. */
    Cvar_Set("ui_ime_target", "");

    int fieldMaxLen = 80;
    if      (strcmp(target, "capturelimit") == 0) fieldMaxLen = 3;

    static wchar_t s_inputBuf[128];
    if (!PS4_ShowOSK(title, fieldMaxLen, s_inputBuf, 128))
        return qtrue; /* OSK was shown but cancelled — absorb the button event */

    char result[128];
    PS4_WcharToASCII(s_inputBuf, result, sizeof(result));
    Cvar_Set("ui_ime_field", target);   /* which field received the result */
    Cvar_Set("ui_ime_text", result);
    Cvar_SetValue("ui_ime_done", 1);
    return qtrue;
}

/* R1+Touchpad: prompt for chat text and send it via `say`. */
static void PS4_ShowChatOSK(void)
{
    static wchar_t s_inputBuf[128];
    static const wchar_t s_title[] = L"Chat";

    if (!PS4_ShowOSK(s_title, 127, s_inputBuf, 128))
        return;

    char msg[128];
    PS4_WcharToASCII(s_inputBuf, msg, sizeof(msg));
#ifdef PS4_DEBUG
    Com_Printf("PS4 Chat: say %s\n", msg);
#endif
    Cbuf_AddText(va("say %s\n", msg));
}

void IN_Init(void *windowData)
{
    int rc;
    (void)windowData;

    Com_Printf("PS4 Input: Initializing DualShock 4...\n");
    
    //set default CVAR if it doesn't exist in q3config.cfg
    Cvar_Get("joy_threshold", "0.0045", CVAR_ARCHIVE); //leave this - it's required for j_pitch/j_yaw settings.

    ps4_aimSensX  = Cvar_Get("ps4_aimSensX", "0.5", CVAR_ARCHIVE);
    ps4_aimSensY  = Cvar_Get("ps4_aimSensY", "0.5", CVAR_ARCHIVE);
    ps4_rumbleEnable     = Cvar_Get("ps4_rumbleEnable",     "1",   CVAR_ARCHIVE);
    ps4_rumbleScale   = Cvar_Get("ps4_rumbleScale",   "1.0", CVAR_ARCHIVE);

    Cbuf_AddText("seta in_joystick 1\n");
    Cvar_Set("in_joystick",          "1");
    Cvar_Set("in_joystickUseAnalog", "1");
    Cvar_Set("j_side_axis",          "0");
    Cvar_Set("j_forward_axis",       "1");
    Cvar_Set("j_pitch_axis",         "3");
    Cvar_Set("j_yaw_axis",           "4");
    Cvar_Get("j_pitch",  "0.0045",  CVAR_ARCHIVE); //up/down
    Cvar_Get("j_yaw",    "-0.0045", CVAR_ARCHIVE); //left/right
    Cvar_Set("j_forward",           "-0.25");
    Cvar_Set("j_side",               "0.25");

    rc = scePadInit();
    if (rc != 0) {
        Com_Printf("WARNING: scePadInit returned 0x%08X (may already be init'd)\n", rc);
    }

    sceUserServiceGetInitialUser(&s_userId);
    Com_Printf("PS4 Input: userId=%d (0x%08X)\n", s_userId, (unsigned)s_userId);

    s_padHandle = scePadOpen(s_userId, ORBIS_PAD_PORT_TYPE_STANDARD, 0, NULL);
    Com_Printf("PS4 Input: scePadOpen returned %d (0x%08X)\n",
               s_padHandle, (unsigned)s_padHandle);

    if (s_padHandle < 0) {
        Com_Printf("WARNING: scePadOpen failed: 0x%08X\n", s_padHandle);
        return;
    }

    memset(&s_padData, 0, sizeof(s_padData));
    memset(ds4_btn_prev, 0, sizeof(ds4_btn_prev));
    memset(ds4_axis_prev, 0, sizeof(ds4_axis_prev));
    ds4_cursor_x = 0.0f;
    ds4_cursor_y = 0.0f;
    s_aimMode    = 0;
    s_aimAccumX  = 0.0f;
    s_aimAccumY  = 0.0f;
    s_touchPrevX = -1.0f;
    s_touchPrevY = -1.0f;
    s_frameCount = 0;
    s_initDone   = 1;
    s_oskActive  = 0;

    s_healthLightbarState  = 0;
    s_healthLightbarLastMs = 0;
    PS4_SetAimModeColor(); /* blue = stick aim (default) */

    Cvar_Get("ui_lastServerAddress", "", CVAR_ARCHIVE);
    Cvar_Get("ui_lastServerPort", "27960", CVAR_ARCHIVE);

    Com_Printf("PS4 Input: DualShock 4 ready (handle %d, user %d)\n",
               s_padHandle, s_userId);
}

void IN_Frame(void)
{
    int ds4_btn_cur[NUM_DS4_BUTTONS];
    int i, rc;

    if (s_padHandle < 0) return;

    /* While the OSK is open, scePadReadState still reports input the OS
       routed to the IME; processing it would duplicate/stick events. */
    if (s_oskActive) {
        PS4_RumbleTick();
        return;
    }

    PS4_UpdateHealthLightbar();

    s_frameCount++;

    rc = scePadReadState(s_padHandle, &s_padData);

    if (rc != 0) {
        if (s_frameCount % 600 == 0) {
            Com_Printf("WARNING: scePadReadState returned 0x%08X\n", rc);
        }
    } else if (s_padData.connected == 0) {
        if (s_frameCount % 600 == 0) {
            Com_Printf("WARNING: Pad disconnected!\n");
        }
    }

    /* First-frame diagnostic: confirms the pad is alive after init. */
    if (s_frameCount == 1) {
        Com_Printf("DS4: rc=%d connected=%d buttons=0x%08X "
                   "LX=%d LY=%d RX=%d RY=%d L2=%d R2=%d\n",
                   rc, (int)s_padData.connected,
                   s_padData.buttons,
                   (int)s_padData.leftStick.x, (int)s_padData.leftStick.y,
                   (int)s_padData.rightStick.x, (int)s_padData.rightStick.y,
                   (int)s_padData.analogButtons.l2,
                   (int)s_padData.analogButtons.r2);
    }

    PS4_RumbleTick();

    /* Read buttons regardless of `connected` -- the field is unreliable. */
    DS4_ReadButtons(&s_padData, ds4_btn_cur);

    int catchers = Key_GetCatcher();
    int in_menu  = (catchers & (KEYCATCH_UI | KEYCATCH_CGAME))         ? 1 : 0;
    int in_text  = (catchers & (KEYCATCH_CONSOLE | KEYCATCH_MESSAGE))  ? 1 : 0;

    /* Console scrolling. Must run before the button loop updates ds4_btn_prev. */
    if (catchers & KEYCATCH_CONSOLE) {
        /* Right stick Y-axis for smooth line-by-line scrolling */
        int ry = (int)s_padData.rightStick.y - STICK_CENTER;
        if (ry > -STICK_DEADZONE && ry < STICK_DEADZONE) ry = 0;

        if (ry != 0) {
            static float scrollAccum = 0.0f;
            float fy = (float)ry / (float)STICK_RANGE;
            if (fy >  1.0f) fy =  1.0f;
            if (fy < -1.0f) fy = -1.0f;

            /* Invert: stick up = scroll up (Con_PageUp), stick down = scroll down */
            scrollAccum += fy * 0.3f; // Increase float value to speed up/slow down scroll speed

            int lines = (int)scrollAccum;
            scrollAccum -= (float)lines;

            while (lines > 0) {
                Con_PageUp();
                lines--;
            }
            while (lines < 0) {
                Con_PageDown();
                lines++;
            }
        }

        // D-Pad Up/Down for page scrolling (edge-triggered)
        uint8_t linejump = 5;
        if (ds4_btn_cur[BTN_IDX_DPAD_UP] && !ds4_btn_prev[BTN_IDX_DPAD_UP]) {
            for (int i = 1; i <= linejump; i++) {
            	Con_PageUp();
            }
        }
        if (ds4_btn_cur[BTN_IDX_DPAD_DOWN] && !ds4_btn_prev[BTN_IDX_DPAD_DOWN]) {
            for (int i = 1; i <= linejump; i++) {
            	Con_PageDown();
            }
        }

        // L1+R1: jump to top/bottom of console buffer
        if (ds4_btn_cur[BTN_IDX_L1] && ds4_btn_cur[BTN_IDX_R1] &&
            (!ds4_btn_prev[BTN_IDX_L1] || !ds4_btn_prev[BTN_IDX_R1])) {
            // Toggle between top and bottom 
            static int atTop = 0;
            if (atTop) {
                Con_Bottom();
                atTop = 0;
            } else {
                Con_Top();
                atTop = 1;
            }
            ds4_btn_prev[BTN_IDX_L1] = 1;
            ds4_btn_prev[BTN_IDX_R1] = 1;
        }
        
        /* Cross: open OSK to type and execute a console command */
        if (ds4_btn_cur[BTN_IDX_CROSS] && !ds4_btn_prev[BTN_IDX_CROSS]) {
            ds4_btn_prev[BTN_IDX_CROSS] = 1;
            PS4_ShowConsoleOSK();
            /* Re-read pad after the OSK flushed state. */
            scePadReadState(s_padHandle, &s_padDataBuf.data);
            DS4_ReadButtons(&s_padData, ds4_btn_cur);
            memcpy(ds4_btn_prev, ds4_btn_cur, sizeof(ds4_btn_prev));
            memset(ds4_axis_prev, 0, sizeof(ds4_axis_prev));
            return;
        }

        /* Circle: close the console */
        if (ds4_btn_cur[BTN_IDX_CIRCLE] && !ds4_btn_prev[BTN_IDX_CIRCLE]) {
            Cbuf_AddText("toggleconsole\n");
            ds4_btn_prev[BTN_IDX_CIRCLE] = 1;
        }
    }

    /* Touchpad combos (edge on Touchpad press; both keys marked seen):
     *   L3 -> toggle stick/touchpad aim, L1 -> console OSK, R1 -> chat OSK */
    {
        int l3_held        = ds4_btn_cur[BTN_IDX_L3];
        int r3_held        = ds4_btn_cur[BTN_IDX_R3];
        int l1_held        = ds4_btn_cur[BTN_IDX_L1];
        int r1_held        = ds4_btn_cur[BTN_IDX_R1];
        int touchpad_press = ds4_btn_cur[BTN_IDX_TOUCHPAD] &&
                             !ds4_btn_prev[BTN_IDX_TOUCHPAD];

        /* L3+Touchpad: toggle stick/touchpad aim. */
        if (touchpad_press && l3_held) {
            s_aimMode    = (s_aimMode == 1) ? 0 : 1;
            s_aimAccumX  = 0.0f;
            s_aimAccumY  = 0.0f;
            s_touchPrevX = -1.0f;
            s_touchPrevY = -1.0f;
            if (s_aimMode == 1) {
                Com_Printf("Aim mode: TOUCHPAD\n");
            } else {
                Com_Printf("Aim mode: STICK\n");
            }
            /* Only update lightbar color if health is not overriding it. */
            if (s_healthLightbarState == 0) {
                PS4_SetAimModeColor();
            }
            ds4_btn_prev[BTN_IDX_L3]      = l3_held;
            ds4_btn_prev[BTN_IDX_TOUCHPAD] = 1;
            goto done_combos;
        }

        /* L1+Touchpad: open the console overlay (always, in any context).
         * Once open, Cross opens the OSK to type a command, Circle closes. */
        if (touchpad_press && l1_held) {
            ds4_btn_prev[BTN_IDX_TOUCHPAD] = 1;
            ds4_btn_prev[BTN_IDX_L1]       = l1_held;
            Cbuf_AddText("toggleconsole\n");
            DS4_ReadButtons(&s_padData, ds4_btn_cur);
            memcpy(ds4_btn_prev, ds4_btn_cur, sizeof(ds4_btn_prev));
            return;
        }
        
        if (touchpad_press && r1_held) {
            ds4_btn_prev[BTN_IDX_TOUCHPAD] = 1;
            PS4_ShowChatOSK();
            /* Re-read pad after the OSK flushed state. */
            scePadReadState(s_padHandle, &s_padDataBuf.data);
            DS4_ReadButtons(&s_padData, ds4_btn_cur);
            memcpy(ds4_btn_prev, ds4_btn_cur, sizeof(ds4_btn_prev));
            memset(ds4_axis_prev, 0, sizeof(ds4_axis_prev));
            return;
        }

        /* L3+R3 (no touchpad): toggle rumble. Edge-detected. */
        if (l3_held && r3_held &&
            (!ds4_btn_prev[BTN_IDX_L3] || !ds4_btn_prev[BTN_IDX_R3])) {
            int on = ps4_rumbleEnable ? !ps4_rumbleEnable->integer : 1;
            Cvar_SetValue("ps4_rumbleEnable", on ? 1.0f : 0.0f);
            Com_Printf("Rumble: %s\n", on ? "ON" : "OFF");
            if (on) {
                PS4_AudioPause();
                PS4_SetRumble(200, 200, 200); /* tactile ack */
                PS4_AudioResume();
            } else {
                OrbisPadVibeParam stop = { 0, 0 };
                scePadSetVibration(s_padHandle, &stop);
                s_rumbleActive   = 0;
                s_rumbleExpiryMs = 0;
            }
            ds4_btn_prev[BTN_IDX_L1] = 1;
            ds4_btn_prev[BTN_IDX_R3] = 1;
            goto done_combos;
        }
        
    }
    done_combos:;

    /* Options+Touchpad: toggle the console overlay. */
    {
        int options_held   = ds4_btn_cur[BTN_IDX_OPTIONS];
        int touchpad_press = ds4_btn_cur[BTN_IDX_TOUCHPAD] &&
                             !ds4_btn_prev[BTN_IDX_TOUCHPAD];
        int combo_consumed = 0;

        if (options_held && touchpad_press) {
            Com_QueueEvent(0, SE_KEY, K_CONSOLE, qtrue,  0, NULL);
            Com_QueueEvent(0, SE_KEY, K_CONSOLE, qfalse, 0, NULL);
            combo_consumed = 1;
            ds4_btn_prev[BTN_IDX_OPTIONS]  = ds4_btn_cur[BTN_IDX_OPTIONS];
            ds4_btn_prev[BTN_IDX_TOUCHPAD] = ds4_btn_cur[BTN_IDX_TOUCHPAD];
        }

        for (i = 0; i < NUM_DS4_BUTTONS; i++) {
            int cur  = ds4_btn_cur[i];
            int prev = ds4_btn_prev[i];

            if (cur == prev) continue;

            if (combo_consumed && (i == BTN_IDX_OPTIONS || i == BTN_IDX_TOUCHPAD))
                goto next_btn;

            /* In menus, send only the synthetic key (Enter/Escape) — not the
             * JOY key as well, or the menu receives two events per press. */
            if (in_menu && (i == BTN_IDX_CROSS || i == BTN_IDX_CIRCLE)) {
                /* Cross press in a menu with an active ui_ime_target field: open OSK instead. */
                if (cur && i == BTN_IDX_CROSS && PS4_ShowFieldOSK()) {
                    /* Re-read pad after the OSK flushed state. */
                    scePadReadState(s_padHandle, &s_padDataBuf.data);
                    DS4_ReadButtons(&s_padData, ds4_btn_cur);
                    memcpy(ds4_btn_prev, ds4_btn_cur, sizeof(ds4_btn_prev));
                    memset(ds4_axis_prev, 0, sizeof(ds4_axis_prev));
                    return;
                }
                Com_QueueEvent(0, SE_KEY, K_ENTER, cur ? qtrue : qfalse, 0, NULL);
            } else if (in_menu && (i == BTN_IDX_SQUARE || i == BTN_IDX_TRIANGLE)) {
                Com_QueueEvent(0, SE_KEY, K_ESCAPE, cur ? qtrue : qfalse, 0, NULL);
            } else {
                Com_QueueEvent(0, SE_KEY, ds4_key_map[i], cur ? qtrue : qfalse, 0, NULL);
            }

        next_btn:
            ds4_btn_prev[i] = cur;
        }
    }

    /* Analog sticks. */
    {
        int lx = (int)s_padData.leftStick.x  - STICK_CENTER;
        int ly = (int)s_padData.leftStick.y  - STICK_CENTER;
        int rx = (int)s_padData.rightStick.x - STICK_CENTER;
        int ry = (int)s_padData.rightStick.y - STICK_CENTER;

        if (lx > -STICK_DEADZONE && lx < STICK_DEADZONE) lx = 0;
        if (ly > -STICK_DEADZONE && ly < STICK_DEADZONE) ly = 0;
        if (rx > -STICK_DEADZONE && rx < STICK_DEADZONE) rx = 0;
        if (ry > -STICK_DEADZONE && ry < STICK_DEADZONE) ry = 0;

        if (in_menu || in_text) {
            /* Left stick drives the menu cursor. */
            if (lx != 0 || ly != 0) {
                float fx = (float)lx / (float)STICK_RANGE;
                float fy = (float)ly / (float)STICK_RANGE;

                if (fx >  1.0f) fx =  1.0f;
                if (fx < -1.0f) fx = -1.0f;
                if (fy >  1.0f) fy =  1.0f;
                if (fy < -1.0f) fy = -1.0f;

                /* Squared response: finer control near center. */
                float ax = fx * fx * MENU_CURSOR_SPEED;
                float ay = fy * fy * MENU_CURSOR_SPEED;
                if (fx < 0.0f) ax = -ax;
                if (fy < 0.0f) ay = -ay;

                ds4_cursor_x += ax;
                ds4_cursor_y += ay;

                int dx = (int)ds4_cursor_x;
                int dy = (int)ds4_cursor_y;
                ds4_cursor_x -= (float)dx;
                ds4_cursor_y -= (float)dy;

                if (dx != 0 || dy != 0)
                    Com_QueueEvent(0, SE_MOUSE, dx, dy, 0, NULL);
            }
        } else {
            /* Gameplay: scale ±(128-deadzone) to Q3's ±32512 short axis range. */
            int jlx = lx * 256;
            int jly = ly * 256;
            int jrx = rx * 256;
            int jry = ry * 256;

            if (jlx != ds4_axis_prev[0]) {
                Com_QueueEvent(0, SE_JOYSTICK_AXIS, 0, jlx, 0, NULL);
                ds4_axis_prev[0] = jlx;
            }
            if (jly != ds4_axis_prev[1]) {
                Com_QueueEvent(0, SE_JOYSTICK_AXIS, 1, jly, 0, NULL);
                ds4_axis_prev[1] = jly;
            }
            if (jrx != ds4_axis_prev[2]) {
                Com_QueueEvent(0, SE_JOYSTICK_AXIS, 4, jrx, 0, NULL);
                ds4_axis_prev[2] = jrx;
            }
            if (jry != ds4_axis_prev[3]) {
                Com_QueueEvent(0, SE_JOYSTICK_AXIS, 3, jry, 0, NULL);
                ds4_axis_prev[3] = jry;
            }
        }
    }

    /* Touchpad aim (mode 1): swipe deltas -> SE_MOUSE events. */
    if (!in_menu && !in_text && s_aimMode == 1) {
        const OrbisPadTouchData *td = &s_padData.touch;
        if (td->fingers > 0) {
            float cx = (float)td->touch[0].x;
            float cy = (float)td->touch[0].y;
            if (s_touchPrevX >= 0.0f) {
                s_aimAccumX += (cx - s_touchPrevX) * ps4_aimSensX->value;
                s_aimAccumY += (cy - s_touchPrevY) * ps4_aimSensY->value;
                int dx = (int)s_aimAccumX;
                int dy = (int)s_aimAccumY;
                s_aimAccumX -= (float)dx;
                s_aimAccumY -= (float)dy;
                if (dx != 0 || dy != 0)
                    Com_QueueEvent(0, SE_MOUSE, dx, dy, 0, NULL);
            }
            s_touchPrevX = cx;
            s_touchPrevY = cy;
        } else {
            s_touchPrevX = -1.0f;
            s_touchPrevY = -1.0f;
        }
    } else {
        s_touchPrevX = -1.0f;
        s_touchPrevY = -1.0f;
    }
}

void IN_Shutdown(void)
{
    if (s_padHandle >= 0) {
        OrbisPadVibeParam v = { 0, 0 };
        scePadSetVibration(s_padHandle, &v);
        scePadClose(s_padHandle);
        s_padHandle = -1;
    }
    s_rumbleActive   = 0;
    s_rumbleExpiryMs = 0;
    s_aimMode    = 0;
    s_aimAccumX  = 0.0f;
    s_aimAccumY  = 0.0f;
    s_touchPrevX = -1.0f;
    s_touchPrevY = -1.0f;
    s_healthLightbarState  = 0;
    s_healthLightbarLastMs = 0;
    s_initDone = 0;
    s_oskActive = 0;
}

void IN_Restart(void)
{
    IN_Shutdown();
    IN_Init(NULL);
}
