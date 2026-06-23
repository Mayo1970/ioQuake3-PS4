# AGENTS.md — ioQuake3-PS4

Quake III Arena / Team Arena / Open Arena port for PS4 (FW 9.00 + GoldHEN), built with OpenOrbis/Clang 18, rendering through Sony's Piglet GLES2/EGL driver. **The port is complete and hardware-verified.** Your job is maintenance, features, or bug triage — not bring-up.

---

## Non-negotiable rules

- **Never rebuild a PKG unprompted.** Wait for an explicit "build it." Automatic or chained builds have produced broken pkgs.
- **Every PS4 change goes behind `#ifdef __ORBIS__`** (or `#ifdef __PS4__`). The same tree must compile for desktop with zero behavioural change.
- **Do not "clean up" hardware-discovered fixes.** If something looks weird, it is weird because the PS4 made it that way. The symptom it cures is in §10 below.
- **Prefer `code/ps4/` for new code.** Hook into shared files with a single guarded call rather than scattering `#ifdef` through engine code.
- **No AI attribution in commits or PRs** (no `Co-Authored-By`, no "Claude/AI" mentions).

---

## Build

**PowerShell:**
```powershell
$env:OO_PS4_TOOLCHAIN = "E:\Users\Matteo\Desktop\quake3\DEVkits\OpenOrbis-PS4-Toolchain"
$env:PATH = "C:\devkitPro\msys2\usr\bin;" + $env:PATH
make -C "E:\Users\Matteo\Desktop\quake3\ioQuake3-PS4\ioQuake3-PS4"           # Q3 release
make -C "..." OA=1     # Open Arena release
make -C "..." TA=1     # Team Arena release
make -C "..." debug    # Q3 debug (writes /data/ioq3/ioquake3log.txt)
make -C "..." all-flavors  # all three release pkgs in sequence
make -C "..." clean    # wipe build/ sce_sys/ qvm/
```

LLVM 18 is auto-detected at `C:\Program Files\LLVM\bin` if `clang` is not on PATH.

**Three variants, one Makefile:**

| Command | Title ID | Base game | Define |
|---|---|---|---|
| `make` | QUAK03000 | baseq3 | *(none)* |
| `make TA=1` | QUAK03001 | baseq3 + `fs_game missionpack` | `-DSTANDALONETA` |
| `make OA=1` | QUAK03002 | baseoa | `-DSTANDALONEOA` |

Per-variant object dirs (`build/obj/{q3,ta,oa}/{release,debug}`) mean you never need `make clean` when switching variants.

**Critical build facts (each one fixed a real failure):**
- All three variants **must use `--paid 0x3800000000000011`**. Any other PAID installs but refuses to boot. Slots are separated by TITLE_ID only.
- `sce_sys/param.sfo` rule is **`.PHONY`-forced**. Without it, variant B reuses variant A's CONTENT_ID → PS4 rejects install with `CE-32945-3`.
- `pkg.gp4` is **hand-written by Makefile `printf` rules**. Do not switch to `create-gp4`; it omits custom dirs and crashes PkgTool.
- `param.sfo` needs `ATTRIBUTE2 0x00000002` for Cross = confirm in the PS4 OSK (non-Japanese layout). Without it, Circle confirms.

---

## Repository layout

```
ioQuake3-PS4/
├── Makefile                    unified build (Q3 / TA / OA)
├── code/
│   ├── ps4/                    ← THE PORT (10 platform files, see §Port layer)
│   ├── qcommon/                patched: q_platform.h, common.c, files.c, net_ip.c, vm.c, qcommon.h
│   ├── renderercommon/         qgl.h — GLES2 macro map
│   ├── renderergl2/            target renderer, GLES2-adapted
│   ├── client/ server/ game/   upstream + surgical #ifdef patches
│   ├── cgame/ q3_ui/ ui/       QVM-side; q3_ui has most PS4 UI/OSK work
│   └── thirdparty/             jpeg-9f, zlib-1.3.1, libogg-1.3.6, libvorbis-1.3.7
├── fixes/                      bundled in PKG; copied to /data/ioq3/ on first boot
│   ├── baseq3/ missionpack/ baseoa/   (each holds pak9-ps4s.pk3 + overrides)
│   └── splash.zip  ta.zip  oa.zip     (per-variant boot splash images)
├── icons/ q3/ ta/ oa/          per-variant icon0.png
├── sce_module/                 user-supplied Sony .sprx (gitignored)
└── ui/                         source for pak9-ps4s.pk3 UI scripts
```

**Reference repos** (read-only): `../ioq3/` (pristine upstream), `vitaQuakeIII/code/psp2/` (Vita port).
**NOT a reference**: `Q3-Legacy/lilium-arena-classic/` — unrelated project.

---

## Port layer (`code/ps4/`)

| File | Role |
|---|---|
| `sys_main_ps4.c` | Entry point: module load, net init, fix installer, command line, `Com_Init`, main loop |
| `sys_ps4.c` | System layer: `Sys_Milliseconds` (gettimeofday), paths (/app0, /data/ioq3), file ops |
| `ps4_glimp.c` | Graphics: Piglet+ShaccVSH load (once), EGL @1920×1080, swap, splash, vid_restart-safe lifecycle |
| `ps4_input.c` | DualShock 4: buttons, sticks, touchpad aim, rumble, lightbar health, IME OSK, default bindings |
| `ps4_snd.c` | DMA audio thread via sceAudioOut; OSK pause/resume |
| `net_ps4.c` | sceNetInit + pool (256 KB) + ctl; caches IP/mask/broadcast |
| `con_ps4.c` | Passive console; log file gated behind `PS4_DEBUG` |
| `user_mem.c` | Custom malloc/mspace backing the engine hunk+zone (critical for boot) |
| `ps4_compat.c` | libc/compat shims |
| `ps4_gamma.c` | No-op (Piglet has no HW gamma) |

---

## Memory model (top cause of boot crashes — `CE-34878-0`)

Two files must stay in sync:

**`qcommon/common.c`** — `#ifdef __ORBIS__` block must contain all three patches:
1. `DEF_COMHUNKMEGS 256` and `MIN_COMHUNKMEGS 256` (default is 16)
2. **40 MB hunk-cap bypass** in `Com_InitHunkMemory` — without it the hunk is silently capped at 40 MB regardless of `DEF_COMHUNKMEGS`
3. `DEF_COMZONEMEGS 48` — upstream raised this to 64; 64 pushes hunk+zone from 304 MB to 320 MB and `calloc` fails

**Before editing `common.c` for any reason, verify all three patches are still present.** The original author hand-edits this file and has removed them accidentally more than once.

**`ps4/user_mem.c`** — mspace try_sizes ladder starts at 512 MB (→384→320→256→192→128). If you raise the hunk or zone size, update this ladder or you get a silent boot crash.

Piglet graphics memory (`ps4_glimp.c`): 250 MB system + 512 MB video + 170 MB flex.

---

## VM / server (infinite "Awaiting Snapshot" fix)

- **`sv_pure` forced to `0`** via `Cvar_Set` in `SV_Init` (`server/sv_init.c`, `#ifdef __ORBIS__`). `+set sv_pure 0` on the command line is insufficient — `CVAR_SYSTEMINFO` cvars are broadcast in the gamestate packet and overwrite the client value.
- **`vm_game/vm_cgame/vm_ui` forced to `1`** (interpreter) via `Cvar_Set` in `VM_Init` (`qcommon/vm.c`, `#ifdef __ORBIS__`). Stale `q3config.cfg` with `vm_cgame 2` overrides any `+set`. JIT (`HAVE_VM_COMPILED`) is not available on PS4.

---

## Networking

Working configuration (after several wrong turns — do not revert):

- **POSIX BSD sockets via `libxnet.a`** (`-lxnet` before `-lSceNet`). Native `sceNetSocket`/`sceNetSendto` etc. return internal handle IDs, not POSIX FDs; calling `ioctl/fcntl/close` on them corrupts kernel resources.
- `PS4_NetInit()` calls `sceNetInit` → `sceNetPoolCreate("ioq3", 256 KB)` → `sceNetCtlInit`. Pool must exist before any socket call. Called **before** `Com_Init` in `sys_main_ps4.c`.
- **sockaddr layout bug:** OpenOrbis `sockaddr_storage.ss_family` is at offset 0 (Linux), but `sockaddr_in.sin_family` is at offset 1 (BSD). `addr.ss_family == AF_INET` always read `sin_len=16` instead of `2`. Fix: dispatch on `netadr_t.type` instead of `ss_family`.
- **Broadcast:** PS4 rejects `255.255.255.255`. Substitute the real subnet broadcast (cached from `sceNetCtlGetInfo`) before `sendto` for `NA_BROADCAST` packets.
- DNS via `sceNetResolverStartNtoa` (timeout 3,000,000 µs). `getaddrinfo` is not available. IPv6 disabled on PS4.

---

## Renderer / GLES2

- **`qgl.h` PS4 path** maps all `qgl*` → `gl*` via macros, stubs desktop functions, defines ~60 missing GL enums, routes DSA functions through `GLDSA_*` bind-then-call fallbacks.
- **GLSL header (`tr_glsl.c`):** `GLSL_GetShaderHeader` must guard the GLSL 1.30 branch with `!qglesMajorVersion`. Piglet reports GLSL ES 2.0 (`major=2`), which tripped the old guard and emitted GLSL 1.30 syntax under `#version 100`, stalling ShaccVSH on every shader. Also: `textureCubeLod` → `textureCube` (GLSL ES 1.00 has no LOD variant); `#line 0` → `#line 1` (ShaccVSH rejects 0).
- **ShaccVSH is strict:** it errors on unreferenced locals. Declare variables inside the guard that uses them; a `var = var;` no-op does not suppress the error.
- **FBOs (`tr_fbo.c`):** depth uses `GL_DEPTH_COMPONENT16` (Piglet returns `GL_FRAMEBUFFER_UNSUPPORTED` for depth-texture attachments). HDR falls back to RGBA8. `glBlitFramebuffer` is GLES3-only → shader-based `FBO_Blit`. Shadow/sunshadow FBOs drop their color renderbuffer; `R_CheckFBO` guarded off with `#ifndef __ORBIS__`.
- **Cinematics:** `RE_UploadCinematic` on `__ORBIS__` uploads directly as `GL_RGBA` — no per-frame `Hunk_AllocateTempMemory` + RGBA→RGB CPU shuffle.
- **Skeletal animation disabled:** `glslMaxAnimatedBones=0`, `gpuVertexAnimation=qfalse` at top of `GLSL_InitGPUShaders` — skips ~36 shader variants.
- **Shader binary cache:** `glGetProgramBinaryOES` entrypoint is NULL on FW 9.00; cache is silently disabled. ~16 s shader compile runs every boot. Splash screen covers the wait.

---

## Boot sequence and lifecycle gotchas

```
main()
 ├─ PS4_LoadSystemModules()   SysCore→Mbus→Ipmi→SystemService→UserService
 │                            →AudioOut→Pad→Net→NetCtl→VideoOut
 │                            (Piglet+ShaccVSH: /data/self/system/common/lib/ with /app0/sce_module/ fallback)
 ├─ PS4_NetInit()             BEFORE Com_Init (pool must pre-exist)
 ├─ PS4_InstallFixes()        /app0/fixes/* → /data/ioq3/ (incremental, marker-gated)
 ├─ build command line        PS4_ADDARG macro — no leading space or intro cinematic is blocked
 └─ Com_Init() → main loop
```

**Gotchas — each one is a real hardware failure:**

- `Sys_Milliseconds` uses `gettimeofday()`. `sceKernelGetProcessTime()` overflows int math on frame 0 → infinite freeze before any render.
- EGL resolution **forced to 1920×1080**. PS4 VideoOut rejects non-standard resolutions at `eglSwapBuffers`. The ioq3 default (1600×1024) fails.
- EGL native-window struct must be **exactly 16 bytes**: `{ uint32_t id, w, h, pad; }`. Any other layout freezes `eglSwapBuffers`.
- **Mod-switch / vid_restart:** `scePigletSetConfigurationVSH` and module load run **once per process** (`s_pigletConfigured` flag). Calling `scePigletSetConfigurationVSH` a second time crashes. `eglTerminate` is **never called** — the EGL display stays alive across restarts; only surface + context are recreated.
- **Command-line leading space:** use `PS4_ADDARG` (prepends a space only when buffer is non-empty). A bare leading space lands in `com_consoleLines[0]`, passes the `Com_AddStartupCommands` filter, returns `qtrue`, and **blocks the intro cinematic**. Same reason: use `+set name`, not `+seta name`.
- Audio user ID: call `sceUserServiceGetInitialUser()` — `0xFF` and `ORBIS_USER_SERVICE_USER_ID_SYSTEM` both return `0x8026000F` for `ORBIS_AUDIO_OUT_PORT_TYPE_MAIN`.
- `OrbisPadData` struct must match the real PS4 ABI padding. Wrong padding → `scePadReadState` writes into adjacent `.bss` → garbage input and hang.

---

## fixes/ installer and the QVM

- Files under `fixes/` are baked into the PKG at `/app0/fixes/` and synced to `/data/ioq3/` on boot by `PS4_InstallFixes()` (incremental: copies only if missing or different size). Marker at `/data/ioq3/fixes_installed_*.txt`; delete via FTP to force reinstall.
- Each variant copies only its folders: Q3→`baseq3/`, TA→`baseq3/`+`missionpack/`, OA→`baseoa/`.
- **The UI/OSK behaviour that runs on hardware is the compiled QVM inside `fixes/*/pak9-ps4s.pk3`, not the C in `code/q3_ui/`.** Editing C alone changes nothing on the console until the QVM is rebuilt and re-shipped. When OSK/menu behaviour regresses, check and rebuild the pk3 first.
- **OSK cvar contract** (`ps4_input.c` ↔ UI QVM): `ui_ime_target` (set while field has focus), `ui_ime_field` (which field), `ui_ime_text` (confirmed string), `ui_ime_done` (1 on confirm). Cross in a menu opens the field OSK for a non-empty `ui_ime_target`; otherwise sends `K_ENTER`.

---

## Input / controls

- **In-game:** L-stick move, R-stick look, R2 fire, L2 zoom, Cross jump, Circle crouch, Square prev-weapon, Triangle next-weapon, L1 strafe-left, R1 use, L3 walk/run, Touchpad scores, Options menu.
- **Menus:** Cross/Circle = `K_ENTER`, Square/Triangle = `K_ESCAPE`. These are mutually exclusive with normal JOY keys to prevent the double-key-event bug.
- **Touchpad aim:** L3+Touchpad toggles stick (lightbar **blue**) vs touchpad aim (lightbar **cyan**, swipe sends `SE_MOUSE` deltas). Cvars: `ps4_aimSensX/Y` (default 0.5). Gyro aim was removed.
- **Rumble:** `ps4_rumbleEnable` (1) / `ps4_rumbleScale` (1.0). Fires only for own weapon fire (per-weapon strength), own pain, hit feedback — matched by sfx name in `PS4_RumbleForSfx`. L3+R3 toggles.
- **Lightbar health (in-game):** hue-alternation (Orbis SDK ignores alpha — no brightness pulsing). 100→50 green→yellow (solid); 50→25 yellow→red (3 s pulse, orange alt); 25→1 red→purple (1 s pulse); ≤0 purple. Health color wins over aim-mode color while in-game.
- **OSK:** L1+Touchpad = console command, R1+Touchpad = chat. `sceImeDialogGetResult()` **must** be called before `sceImeDialogTerm()`. Audio paused (`PS4_AudioPause`) while OSK is open; buttons unstuck and input flushed on close.

---

## Standalone variants

| Variant | `BASEGAME` | Extra at boot | Data needed on PS4 |
|---|---|---|---|
| ioQuake 3 | `baseq3` | — | `/data/ioq3/baseq3/pak0–8.pk3` |
| Open Arena | `baseoa` | — | `/data/ioq3/baseoa/pak*.pk3` |
| Team Arena | `baseq3` | `+set fs_game missionpack` | baseq3 pak0–8 **+** missionpack pak0–3 |

TA is a **mod layered on baseq3**, not a true standalone. Setting `BASEGAME="missionpack"` cuts baseq3 out of the loader and crashes `CE-34878-0` at `files.c:3750`. Mission-pack `pak1–3` must be the **point-release files** from an ioquake3 PC install (Steam TA ships only `pak0`; the QVM validates checksums).

---

## Known-incomplete items

- **Banner Z-fighting** — 16-bit FBO depth has ~256× less precision than desktop. `glPolygonOffset` is a **no-op on Piglet**. Intended fix: shader-override `pak10.pk3` with `polygonOffset` + `sort banner` keywords in `fixes/baseq3/`. Pending.
- **sRGB textures** — enums defined for compile, non-functional on GLES2. Harmless for gameplay.
- **Shadow mapping** — depth-only FBOs unsupported on Piglet → silently no shadows, no crash.
- **Shader binary cache** — `glGetProgramBinaryOES` is NULL on FW 9.00 (§Renderer).

---

## Checklist before touching the tree

1. Read this file and `README.md`.
2. Confirm the three `common.c` memory patches (§Memory model) are present.
3. If changing hunk/zone size, update `user_mem.c` try_sizes too.
4. If touching UI/OSK/menus, rebuild the QVM in `fixes/*/pak9-ps4s.pk3` — C edits alone don't reach the hardware.
5. Keep every change behind `#ifdef __ORBIS__`; prefer adding to `code/ps4/`.
6. Don't add OpenAL / curl / VoIP / SDL-GL / JIT — deliberately excluded.
7. No AI attribution in commits.
8. Don't build a PKG until explicitly asked.

---

*Port complete and hardware-verified across all three variants (FW 9.00, GoldHEN). Last updated 2026-06-05.*
