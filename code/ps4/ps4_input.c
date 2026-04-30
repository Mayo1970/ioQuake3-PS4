/* ps4_input.c -- DualShock 4 input via scePad. */

#include <string.h>
#include <wchar.h>
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

/* Only bind if not already bound (preserves q3config.cfg overrides) */
static void PS4_SetDefaultBind(int keynum, const char *keyname, const char *binding)
{
    char *existing = Key_GetBinding(keynum);
    if (!existing || !existing[0]) {
        Key_SetBinding(keynum, binding);
        /* Force it into command buffer so it saves to config */
        Cbuf_AddText(va("bind %s \"%s\"\n", keyname, binding));
    }
}

/* ------------------------------------------------------------------ */
/* Constants                                                            */
/* ------------------------------------------------------------------ */

#define STICK_CENTER    128
#define STICK_DEADZONE  30
#define STICK_RANGE     (128 - STICK_DEADZONE)  /* effective half-range */

#define MENU_CURSOR_SPEED 5.0f   /* pixels per frame at full deflection */

#define TRIGGER_THRESHOLD 30     /* analogButtons.l2/r2 threshold (0-255) */

/* Number of logical buttons tracked (14 digital + 2 analog triggers) */
#define NUM_DS4_BUTTONS 16

/* Button indices into ds4_btn_cur[] / ds4_btn_prev[] */
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
    K_JOY1,       /*  0: Cross    -> jump         */
    K_JOY2,       /*  1: Circle   -> crouch        */
    K_JOY3,       /*  2: Square   -> prev weapon   */
    K_JOY4,       /*  3: Triangle -> next weapon   */
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

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

static int          s_padHandle  = -1;
static int          s_userId     = -1;

/* Add padding to prevent stack/bss corruption if FW 9.00 PadData is larger than the header struct */
static union {
    OrbisPadData data;
    uint8_t pad[256];
} s_padDataBuf;
#define s_padData (s_padDataBuf.data)

static int   ds4_btn_prev[NUM_DS4_BUTTONS];
static int   ds4_axis_prev[4];           /* LX LY RX RY last sent value */
static float ds4_cursor_x = 0.0f;
static float ds4_cursor_y = 0.0f;

static int s_frameCount  = 0;            /* diagnostic frame counter */
static int s_initDone    = 0;

/* ------------------------------------------------------------------ */
/* Read button states into flat array (matches OpenOrbis controller.cpp)
 * ------------------------------------------------------------------ */
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

    /* Trigger: digital bit OR analog pressure above threshold */
    out[BTN_IDX_L2_ANALOG] = ((b & ORBIS_PAD_BUTTON_L2) ||
                               pd->analogButtons.l2 > TRIGGER_THRESHOLD) ? 1 : 0;
    out[BTN_IDX_R2_ANALOG] = ((b & ORBIS_PAD_BUTTON_R2) ||
                               pd->analogButtons.r2 > TRIGGER_THRESHOLD) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* PS4_ShowOSK (internal helper)                                       */
/* Returns qtrue if the user confirmed with non-empty text.            */
/* ------------------------------------------------------------------ */
static qboolean PS4_ShowOSK(const wchar_t *title, int maxLen,
                             wchar_t *buf, int bufLen)
{
    memset(buf, 0, bufLen * sizeof(wchar_t));

    OrbisImeDialogSetting setting;
    memset(&setting, 0, sizeof(setting));
    setting.userId              = s_userId;
    setting.type                = ORBIS_TYPE_BASIC_LATIN;
    setting.enterLabel          = ORBIS_BUTTON_LABEL_GO;
    setting.option              = 0;
    setting.maxTextLength       = maxLen;
    setting.inputTextBuffer     = buf;
    setting.posx                = 960.0f;
    setting.posy                = 540.0f;
    setting.horizontalAlignment = ORBIS_H_CENTER;
    setting.verticalAlignment   = ORBIS_V_CENTER;
    setting.title               = title;

    int ret = sceImeDialogInit(&setting, NULL);
    if (ret < 0) {
        Com_Printf("PS4 OSK: sceImeDialogInit failed: 0x%08X\n", (unsigned)ret);
        return qfalse;
    }

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
    return confirmed;
}

/* Converts a wchar_t OSK result to a plain ASCII C string. Non-ASCII
 * characters are replaced with '?' so the engine command parser is safe. */
static void PS4_WcharToASCII(const wchar_t *src, char *dst, int dstLen)
{
    int i;
    for (i = 0; i < dstLen - 1 && src[i]; i++)
        dst[i] = (src[i] < 0x80) ? (char)src[i] : '?';
    dst[i] = '\0';
}

/* ------------------------------------------------------------------ */
/* PS4_ShowConsoleOSK                                                  */
/* L1 + Touchpad: open OSK, execute typed text as a console command.  */
/* ------------------------------------------------------------------ */
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

/* ------------------------------------------------------------------ */
/* PS4_ShowChatOSK                                                     */
/* R1 + Touchpad: open OSK, send typed text as an in-game chat msg.   */
/* ------------------------------------------------------------------ */
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

/* ------------------------------------------------------------------ */
/* IN_Init                                                             */
/* ------------------------------------------------------------------ */
void IN_Init(void *windowData)
{
    int rc;
    (void)windowData;

    Com_Printf("PS4 Input: Initializing DualShock 4...\n");

    /* Default gameplay binds (preserved if already in q3config.cfg) */
    PS4_SetDefaultBind(K_JOY1, "JOY1", "+moveup");      /* Cross    = jump        */
    PS4_SetDefaultBind(K_JOY2, "JOY2", "+movedown");    /* Circle   = crouch      */
    PS4_SetDefaultBind(K_JOY3, "JOY3", "weapprev");     /* Square   = prev weapon */
    PS4_SetDefaultBind(K_JOY4, "JOY4", "weapnext");     /* Triangle = next weapon */
    PS4_SetDefaultBind(K_JOY5, "JOY5", "+moveleft");    /* L1       = strafe left */
    PS4_SetDefaultBind(K_JOY6, "JOY6", "+moveright");   /* R1       = strafe right*/
    PS4_SetDefaultBind(K_JOY7, "JOY7", "+zoom");        /* L2       = zoom        */
    PS4_SetDefaultBind(K_JOY8, "JOY8", "+attack");      /* R2       = fire        */
    PS4_SetDefaultBind(K_JOY9, "JOY9", "+speed");       /* L3       = walk toggle */
    PS4_SetDefaultBind(K_JOY10,"JOY10", "+scores");     /* R3       = scoreboard  */
    PS4_SetDefaultBind(K_JOY11,"JOY11", "+scores");     /* Touchpad = scoreboard  */

    /* Enable joystick axis mode for twin-stick FPS movement / camera */
    Cbuf_AddText("seta in_joystick 1\n");
    Cvar_Set("in_joystick",          "1");
    Cvar_Set("in_joystickUseAnalog", "1");
    Cvar_Set("j_side_axis",          "0");   /* left stick X  -> strafe  */
    Cvar_Set("j_forward_axis",       "1");   /* left stick Y  -> forward */
    Cvar_Set("j_pitch_axis",         "3");   /* right stick Y -> pitch   */
    Cvar_Set("j_yaw_axis",           "4");   /* right stick X -> yaw     */
    Cvar_Set("j_pitch",              "0.022");
    Cvar_Set("j_yaw",               "-0.020"); /* negative = non-inverted */
    Cvar_Set("j_forward",           "-0.25");
    Cvar_Set("j_side",               "0.25");

    /* scePadInit: OpenOrbis sample checks != 0 (not just negative) */
    rc = scePadInit();
    if (rc != 0) {
        Com_Printf("WARNING: scePadInit returned 0x%08X (may already be init'd)\n", rc);
        /* Continue anyway -- may already be initialized by system */
    }

    /* Get the initial logged-in user */
    {
        sceUserServiceGetInitialUser(&s_userId);
    }

    Com_Printf("PS4 Input: userId=%d (0x%08X)\n", s_userId, (unsigned)s_userId);

    /* Open pad handle -- OpenOrbis sample uses type=0, index=0, param=NULL */
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
    s_frameCount = 0;
    s_initDone   = 1;

    Com_Printf("PS4 Input: DualShock 4 ready (handle %d, user %d)\n",
               s_padHandle, s_userId);
}

/* ------------------------------------------------------------------ */
/* IN_Frame                                                            */
/* ------------------------------------------------------------------ */
void IN_Frame(void)
{
    int ds4_btn_cur[NUM_DS4_BUTTONS];
    int i, rc;

    if (s_padHandle < 0) return;

    s_frameCount++;

    /* Read pad state */
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

    /* Log pad state on first frame only to confirm controller is alive */
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

    /* Process input regardless of connected field (OpenOrbis controller.cpp
     * does not check connected -- just reads buttons directly). */
    DS4_ReadButtons(&s_padData, ds4_btn_cur);

    int catchers = Key_GetCatcher();
    int in_menu  = (catchers & (KEYCATCH_UI | KEYCATCH_CGAME))         ? 1 : 0;
    int in_text  = (catchers & (KEYCATCH_CONSOLE | KEYCATCH_MESSAGE))  ? 1 : 0;

    /* Touchpad combos (L1/R1 held as modifier, Touchpad as trigger):
     *   L1 + Touchpad -> console OSK (type and execute a command)
     *   R1 + Touchpad -> chat OSK    (type and send a chat message)
     * L1 takes priority if both are held simultaneously. */
    {
        int l1_held        = ds4_btn_cur[BTN_IDX_L1];
        int r1_held        = ds4_btn_cur[BTN_IDX_R1];
        int touchpad_press = ds4_btn_cur[BTN_IDX_TOUCHPAD] &&
                             !ds4_btn_prev[BTN_IDX_TOUCHPAD];

        if (touchpad_press && l1_held) {
            ds4_btn_prev[BTN_IDX_TOUCHPAD] = 1;
            PS4_ShowConsoleOSK();
            DS4_ReadButtons(&s_padData, ds4_btn_cur);
            memcpy(ds4_btn_prev, ds4_btn_cur, sizeof(ds4_btn_prev));
            return;
        }
        if (touchpad_press && r1_held) {
            ds4_btn_prev[BTN_IDX_TOUCHPAD] = 1;
            PS4_ShowChatOSK();
            DS4_ReadButtons(&s_padData, ds4_btn_cur);
            memcpy(ds4_btn_prev, ds4_btn_cur, sizeof(ds4_btn_prev));
            return;
        }
    }

    /* Options + Touchpad = toggle console */
    {
        int options_held   = ds4_btn_cur[BTN_IDX_OPTIONS];
        int touchpad_press = ds4_btn_cur[BTN_IDX_TOUCHPAD] &&
                             !ds4_btn_prev[BTN_IDX_TOUCHPAD];
        int combo_consumed = 0;

        if (options_held && touchpad_press) {
            Com_QueueEvent(0, SE_KEY, K_CONSOLE, qtrue,  0, NULL);
            Com_QueueEvent(0, SE_KEY, K_CONSOLE, qfalse, 0, NULL);
            combo_consumed = 1;
            /* Mark both as "seen" so they don't fire their individual keys */
            ds4_btn_prev[BTN_IDX_OPTIONS]  = ds4_btn_cur[BTN_IDX_OPTIONS];
            ds4_btn_prev[BTN_IDX_TOUCHPAD] = ds4_btn_cur[BTN_IDX_TOUCHPAD];
        }

        /* Digital button edge detection */
        for (i = 0; i < NUM_DS4_BUTTONS; i++) {
            int cur  = ds4_btn_cur[i];
            int prev = ds4_btn_prev[i];

            if (cur == prev) continue;

            /* Suppress individual keys consumed by the combo above */
            if (combo_consumed && (i == BTN_IDX_OPTIONS || i == BTN_IDX_TOUCHPAD))
                goto next_btn;

            Com_QueueEvent(0, SE_KEY, ds4_key_map[i], cur ? qtrue : qfalse, 0, NULL);

            /* Cross in menu also sends K_ENTER for UI navigation */
            if (i == BTN_IDX_CROSS && in_menu)
                Com_QueueEvent(0, SE_KEY, K_ENTER, cur ? qtrue : qfalse, 0, NULL);

            /* Circle in menu also sends K_ESCAPE for back navigation */
            if (i == BTN_IDX_CIRCLE && in_menu)
                Com_QueueEvent(0, SE_KEY, K_ESCAPE, cur ? qtrue : qfalse, 0, NULL);

        next_btn:
            ds4_btn_prev[i] = cur;
        }
    }

    /* ---- Analog sticks ---- */
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
            /* Left stick navigates menu cursor */
            if (lx != 0 || ly != 0) {
                float fx = (float)lx / (float)STICK_RANGE;
                float fy = (float)ly / (float)STICK_RANGE;

                if (fx >  1.0f) fx =  1.0f;
                if (fx < -1.0f) fx = -1.0f;
                if (fy >  1.0f) fy =  1.0f;
                if (fy < -1.0f) fy = -1.0f;

                /* Squared response for fine control at small deflections */
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
            /* Gameplay: both sticks -> joystick axes.
             * Scale ±(128-deadzone) to ±32512 for Q3's short axis range. */
            int jlx = lx * 256;
            int jly = ly * 256;
            int jrx = rx * 256;
            int jry = ry * 256;

            /* Send only on change to avoid flooding the queue */
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
}

/* ------------------------------------------------------------------ */
/* IN_Shutdown / IN_Restart                                            */
/* ------------------------------------------------------------------ */
void IN_Shutdown(void)
{
    if (s_padHandle >= 0) {
        scePadClose(s_padHandle);
        s_padHandle = -1;
    }
    s_initDone = 0;
}

void IN_Restart(void)
{
    IN_Shutdown();
    IN_Init(NULL);
}
