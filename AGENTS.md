# AGENTS.md — ioQuake3-PS4

Quake III Arena / Team Arena / Open Arena / Quake 3 Classic port for PS4 (FW 9.00 + GoldHEN),
built with the OpenOrbis toolchain (Clang 18), rendering through Sony's Piglet GLES2/EGL driver.
**The port is complete and hardware-verified across all four variants.** Your job is maintenance,
features, or bug triage — not bring-up.

> **This file is the single source of truth for the project.** `CLAUDE.md` is a thin pointer to it.
> Keep it concise and *current* — describe how the code works *now*, not a history of fixes. Git log is
> the changelog; do not turn this file into one. When a fact here drifts from the code, fix the fact.

---

## Non-negotiable rules

- **Never rebuild a PKG unprompted.** Wait for an explicit "build it." Editing metadata/icons/Makefile
  does *not* imply a rebuild.
- **Every PS4 change goes behind `#ifdef __ORBIS__`** (or `__PS4__`). The same tree must still compile
  for desktop with zero behavioural change. CLASSIC-only changes go behind `#ifdef CLASSIC`.
- **Do not "clean up" hardware-discovered fixes.** If something looks weird, it is weird because the PS4
  made it that way — the symptom it cures is documented below.
- **Prefer `code/ps4/` for new code.** Hook into shared files with a single guarded call rather than
  scattering `#ifdef` through engine code.
- **No AI attribution in commits or PRs** (no `Co-Authored-By`, no "Claude/AI" mentions).

---

## Build

**PowerShell:**
```powershell
$env:OO_PS4_TOOLCHAIN = "E:\Users\Matteo\Desktop\quake3\DEVkits\OpenOrbis-PS4-Toolchain"
$env:PATH = "C:\devkitPro\msys2\usr\bin;" + $env:PATH
$proj = "E:\Users\Matteo\Desktop\quake3\ioQuake3-PS4\ioQuake3-PS4"
make -C $proj                 # Q3 release          (or: make -C $proj release)
make -C $proj ta              # Team Arena release  (== TA=1)
make -C $proj oa              # Open Arena release  (== OA=1)
make -C $proj classic         # Quake 3 Classic     (== CLASSIC=1)
make -C $proj debug           # Q3 debug (writes /data/ioq3/ioquake3log.txt)
make -C $proj all-flavors     # all four release pkgs in sequence
make -C $proj clean           # wipe build/ sce_sys/ pkg artifacts
```

LLVM 18 is auto-detected at `C:\Program Files\LLVM\bin` if `clang` is not on PATH.
Toolchain lives at `…\DEVkits\OpenOrbis-PS4-Toolchain` (note: **DEVkits**, not devkit).

**Four variants, one Makefile** (flavor selected by `make <flavor>` shorthand or `XX=1`):

| Command | Title ID | Base game | Boot extra | Define |
|---|---|---|---|---|
| `make` | QUAK03000 | baseq3 | — | *(none)* |
| `make ta` | QUAK03001 | baseq3 | `+set fs_game missionpack` | `-DSTANDALONETA` |
| `make oa` | QUAK03002 | baseoa | — | `-DSTANDALONEOA` |
| `make classic` | QUAK03003 | baseq3 | legacy proto 43 | `-DCLASSIC -DLEGACY_PROTOCOL` |

Per-variant object dirs (`build/obj/{q3,ta,oa,classic}/{release,debug}`) plus a `build/.flavor_*` stamp
mean you never need `make clean` when switching variants — and switching forces a relink of shared
objects so standalone defines can't go stale.

**Critical build facts (each one fixed a real failure — do not "improve" them):**
- All variants **must use `--paid 0x3800000000000011`**. Any other PAID installs but refuses to boot.
  Homebrew slots are separated by **TITLE_ID only**; distinct PAIDs are not needed and break booting.
- `sce_sys/param.sfo` rule is **`.PHONY`-forced**. The single shared sfo is stamped per-variant with its
  CONTENT_ID; without the force, variant B reuses variant A's CONTENT_ID → install fails `CE-32945-3`.
- `pkg.gp4` is **hand-written by Makefile `printf` rules**. Do not switch to `create-gp4`; it omits the
  custom `fixes/` dirs from `<rootdir>` and crashes PkgTool ("Sequence contains no elements").
- `param.sfo` needs `ATTRIBUTE 0x00000042` + `ATTRIBUTE2 0x00000002` (and the 7 empty
  `SERVICE_ID_ADDCONT_ADD_*` entries) so **Cross = confirm** in the PS4 OSK/system UI (non-JP layout).
  Without `ATTRIBUTE2`, Circle confirms. `REMOTE_PLAY_KEY_ASSIGN` is unrelated to this.
- Release strips debug info (`llvm-strip --strip-debug` before `create-fself`) and uses `-O2`; debug uses
  `-O0 -g` and defines `PS4_DEBUG`. Each variant bundles only its own splash zip (~3–4 MB PKGs).
- `PkgTool.Core.runtimeconfig.json` has `"rollForward": "LatestMajor"` so the .NET Core 3.0 tool runs on
  installed .NET 6+.

---

## Repository layout

```
ioQuake3-PS4/
├── Makefile                    unified build (Q3 / TA / OA / Classic)
├── code/
│   ├── ps4/                    ← THE PORT (11 platform files, see §Port layer)
│   ├── qcommon/                patched: q_platform.h, common.c, files.c, net_ip.c, vm.c, qcommon.h, ioapi/unzip
│   ├── renderercommon/         qgl.h — GLES2 macro map
│   ├── renderergl2/            target renderer, GLES2-adapted
│   ├── client/ server/         upstream + surgical #ifdef patches
│   ├── cgame/ q3_ui/ ui/       QVM-side; q3_ui holds most PS4 UI/OSK work
│   ├── botlib/ asm/            upstream, unmodified
│   └── thirdparty/             jpeg-9f, zlib-1.3.1, libogg-1.3.6, libvorbis-1.3.7 (bundled, no system deps)
├── fixes/                      bundled in PKG at /app0/fixes/; synced to /data/ioq3/ on boot
│   ├── baseq3/                 pak9-ps4.pk3 (Q3/TA OSK+UI), zpack-classic.pk3 (Classic QVMs/assets)
│   ├── missionpack/            pak4-ps4.pk3 (TA OSK fields)
│   ├── baseoa/                 (OA overrides — may be empty in-repo; OA pk3s are gitignored)
│   └── splash.zip ta.zip oa.zip   per-variant 1920×1080 boot splash images
├── icons/  q3/ ta/ oa/ qc/ ra/ per-variant icon0.png (qc=Classic; ra is staged but NOT wired into Makefile)
├── sce_module/                 user-supplied Sony .sprx (Piglet + ShaccVSH), gitignored
├── sce_sys/                    generated PKG metadata (param.sfo, icon0.png)
└── ui/                         source for the pak9-ps4s UI scripts (hud.txt, menus.txt, …)
```

**Reference repos** (read-only): `../ioq3/` (pristine upstream), `vitaQuakeIII/code/psp2/` (Vita port).
`Q3-Legacy/lilium-arena-classic/` is a valid reference **only for CLASSIC / protocol-43 compat logic**
(its `#ifdef ELITEFORCE` ≈ our `#ifdef CLASSIC`) — **never** for PS4-specific or renderer code.

---

## Port layer (`code/ps4/`)

| File | Role |
|---|---|
| `sys_main_ps4.c` | Entry point: module load → net init → fix installer → command line → `Com_Init` → main loop |
| `sys_ps4.c` | System layer: `Sys_Milliseconds` (gettimeofday), paths (/app0, /data/ioq3), file/dir ops |
| `ps4_glimp.c` | Graphics: Piglet+ShaccVSH load (once), EGL @1920×1080, swap, boot splash, vid_restart-safe lifecycle |
| `ps4_input.c` | DualShock 4: buttons, sticks, touchpad aim, rumble, lightbar health, IME OSK, default bindings |
| `ps4_snd.c` | DMA audio thread via sceAudioOut; OSK pause/resume |
| `net_ps4.c` | sceNetInit + pool (256 KB) + ctl; caches IP/mask/broadcast |
| `con_ps4.c` | Passive console; log file gated behind `PS4_DEBUG` |
| `user_mem.c/.h` | Custom malloc/mspace backing the engine hunk+zone (critical for boot) |
| `ps4_compat.c` | libc/compat shims |
| `ps4_gamma.c` | No-op (Piglet has no HW gamma) |

---

## Memory model (top cause of boot crashes — `CE-34878-0`)

Two files must stay in sync.

**`qcommon/common.c`** — the `#ifdef __ORBIS__` block must contain all three patches:
1. `DEF_COMHUNKMEGS 256` and `MIN_COMHUNKMEGS 256` (upstream default is 16).
2. **40 MB hunk-cap bypass** in `Com_InitHunkMemory` — without it the hunk is silently capped at 40 MB
   regardless of `DEF_COMHUNKMEGS` (`HUNK_ALLOC FAILED` at map load, `tr_model.c`).
3. `DEF_COMZONEMEGS 48` — upstream raised this to 64; 64 pushes hunk+zone from 304→320 MB, `calloc`
   for the zone returns NULL → boot crash.

> **Before editing `common.c` for any reason, verify all three patches are still present.** The original
> author hand-edits this file and has accidentally removed them more than once.

**`ps4/user_mem.c`** — the mspace `try_sizes` ladder starts at 512 MB (→384→320→256→192→128). If you
raise the hunk or zone, raise this ladder too or you get a silent boot crash.

Piglet graphics memory (`ps4_glimp.c`): 250 MB system + 512 MB video + 170 MB flex ≈ 932 MB.

---

## VM / server (infinite "Awaiting Snapshot" fixes)

- **`sv_pure` forced to `0`** via `Cvar_Set` in `SV_Init` (`server/sv_init.c`, `#ifdef __ORBIS__`).
  `+set sv_pure 0` on the command line is insufficient — `CVAR_SYSTEMINFO` cvars are broadcast in the
  gamestate and overwrite the client value, so the local client never authenticates and the server
  re-sends the full gamestate forever.
- **`vm_game/vm_cgame/vm_ui` forced to `1`** (interpreter) via `Cvar_Set` in `VM_Init`
  (`qcommon/vm.c`, `#ifdef __ORBIS__`). JIT (`HAVE_VM_COMPILED`) is not built on PS4; a stale
  `q3config.cfg` with `vm_cgame 2` would otherwise hang cgame load at `CA_PRIMED`.

---

## Networking

Working configuration (after several wrong turns — do not revert):

- **POSIX BSD sockets via `libxnet.a`** (`-lxnet` before `-lSceNet`). Native `sceNetSocket`/`sceNetSendto`
  etc. return internal handle IDs, **not** POSIX FDs; calling `ioctl/fcntl/close` on them corrupts
  unrelated kernel resources. Everything uses `socket()/bind()/sendto()/recvfrom()/setsockopt()/close()`
  and `fcntl(F_SETFL, O_NONBLOCK)`; `errno` is correct on POSIX FDs.
- `PS4_NetInit()`: `sceNetInit` → `sceNetPoolCreate("ioq3", 256 KB)` → `sceNetCtlInit`. The pool must
  exist before any socket call (else `ENOENT`). Called **before** `Com_Init` in `sys_main_ps4.c`.
- **sockaddr layout bug:** OpenOrbis `sockaddr_storage.ss_family` is at offset 0 (Linux), but
  `sockaddr_in.sin_family` is at offset 1 (BSD `sin_len` at 0). Every `addr.ss_family == AF_INET` read
  `sin_len=16` instead of `2` → `sendto` was never called. Fix: dispatch on `netadr_t.type`
  (`NA_BROADCAST`/`NA_IP`), never `ss_family`.
- **Broadcast:** PS4 rejects `255.255.255.255`. Substitute the real subnet broadcast (cached from
  `sceNetCtlGetInfo` IP+netmask) before `sendto` for `NA_BROADCAST`. `NET_GetLocalAddress` populates
  `localIP[]` from the same cache.
- DNS via `sceNetResolverStartNtoa` (timeout 3,000,000 µs). `getaddrinfo` is unavailable → connect by IP.
  IPv6 disabled on PS4; SOCKS disabled.

---

## Renderer / GLES2 (Piglet)

- **`qgl.h` PS4 path** maps all `qgl*` → `gl*` via macros, stubs desktop-only functions, defines ~60
  missing GL enums (sRGB, sized/float/depth/compressed formats, shadow/compare, anisotropic, VAO query,
  depth clamp, cubemap wrap, multisample), and routes DSA functions through `GLDSA_*` bind-then-call
  fallbacks (`tr_dsa.c`).
- **GLSL header (`tr_glsl.c`):** `GLSL_GetShaderHeader` **must** guard the GLSL 1.30 branch with
  `!qglesMajorVersion`. Piglet reports GLSL ES 2.0 (`major=2`), which tripped the old guard and emitted
  GLSL 1.30 syntax under `#version 100`, stalling ShaccVSH on every shader. Also: `textureCubeLod` →
  `textureCube` (ES 1.00 has no LOD variant); `#line 0` → `#line 1` (ShaccVSH rejects 0 as non-positive).
- **ShaccVSH is strict:** it errors on unreferenced locals. Declare a variable inside the `#if` guard that
  actually uses it; a `var = var;` no-op does **not** suppress the error (see `generic_vp.glsl`).
- **FBOs (`tr_fbo.c`):** depth uses `GL_DEPTH_COMPONENT16` (Piglet returns `GL_FRAMEBUFFER_UNSUPPORTED`
  for depth-*texture* attachments despite advertising `GL_OES_depth_texture`). HDR falls back to RGBA8.
  `glBlitFramebuffer` is GLES3-only → shader-based `FBO_Blit`. Shadow/sunshadow FBOs drop their color
  renderbuffer; `R_CheckFBO` is guarded off with `#ifndef __ORBIS__` (objects still created — `tr_main.c`
  dereferences them without null checks).
- **Cinematics:** `RE_UploadCinematic` on `__ORBIS__` uploads the RoQ buffer directly as `GL_RGBA`
  (Piglet has `GL_OES_rgb8_rgba8`) — no per-frame `Hunk_AllocateTempMemory` + RGBA→RGB CPU shuffle.
  Fixed OA boot cinematic + TA main-menu background RoQ stutter.
- **Skeletal animation disabled:** `glslMaxAnimatedBones=0`, `gpuVertexAnimation=qfalse` at the top of
  `GLSL_InitGPUShaders` — skips ~36 shader variants. Bone count auto-clamps from
  `GL_MAX_VERTEX_UNIFORM_VECTORS`; below 12 bones it disables entirely.
- **Shader binary cache:** `glGetProgramBinaryOES` is NULL on FW 9.00's Piglet → cache silently disabled,
  so the ~16 s shader compile runs every boot. The boot splash covers the wait.

---

## Boot sequence and lifecycle gotchas

```
main()
 ├─ PS4_LoadSystemModules()   SysCore→Mbus→Ipmi→SystemService→UserService
 │                            →AudioOut→Pad→Net→NetCtl→VideoOut
 │                            (Piglet+ShaccVSH: /data/self/system/common/lib/ → /app0/sce_module/ fallback)
 ├─ PS4_NetInit()             BEFORE Com_Init (pool must pre-exist)
 ├─ PS4_InstallFixes()        /app0/fixes/* → /data/ioq3/  (see §fixes installer)
 ├─ build command line        PS4_ADDARG macro — no leading space, or the intro cinematic is blocked
 └─ Com_Init() → main loop
```

**Each gotcha below is a real hardware failure:**

- `Sys_Milliseconds` uses `gettimeofday()`. `sceKernelGetProcessTime()` overflows the int math on frame 0
  → infinite freeze before any render. `sys_timeBase` is set in `Sys_PlatformInit`.
- EGL resolution **forced to 1920×1080**. PS4 VideoOut rejects non-standard resolutions at
  `eglSwapBuffers`. The ioq3 default (1600×1024) fails. `r_customwidth/height` and `r_mode -1` are set
  before EGL init.
- EGL native-window struct must be **exactly 16 bytes**: `{ uint32_t id, w, h, pad; }`. Any other layout
  (OrbisPglWindow, 3-int) freezes `eglSwapBuffers`. `eglSwapInterval(0)`.
- **vid_restart / mod switch:** `scePigletSetConfigurationVSH` and module load run **once per process**
  (`s_pigletConfigured` flag) — a second `scePigletSetConfigurationVSH` crashes. `eglTerminate` is
  **never called** (Piglet can't re-init); the EGL display stays alive across restarts, only surface +
  context are recreated. `GLimp_Shutdown`/`GLimp_Init` call `ri.IN_Shutdown()`/`ri.IN_Init(NULL)`
  symmetrically so the pad handle reopens cleanly.
- **Command-line leading space:** use `PS4_ADDARG` (prepends a space only when the buffer is non-empty).
  A bare leading space lands in `com_consoleLines[0]`, passes the `Com_AddStartupCommands` filter, returns
  `qtrue`, and **blocks the intro cinematic**. Same reason: use `+set name`, not `+seta name` (`seta `
  slips past the `set ` filter).
- Audio user ID: call `sceUserServiceGetInitialUser()` — both `0xFF` and
  `ORBIS_USER_SERVICE_USER_ID_SYSTEM` return `0x8026000F` for `ORBIS_AUDIO_OUT_PORT_TYPE_MAIN`.
- `OrbisPadData` struct must match the real PS4 ABI padding. Wrong padding → `scePadReadState` writes into
  adjacent `.bss` → garbage input and a hang.

---

## fixes/ installer and the QVM

- Files under `fixes/` are baked into the PKG at `/app0/fixes/` and synced to `/data/ioq3/` **on every
  boot** by `PS4_InstallFixes()` (`sys_main_ps4.c`). The sync is **incremental, not marker-gated**:
  `PS4_SyncDir` recurses and copies a file only when it's missing or a **different size**
  (`PS4_FileNeedsUpdate`). There is **no `fixes_installed` marker file** — bump a file's size to force it
  to recopy, or delete the target via FTP.
- Each variant copies only its folders: **Q3 / Classic** → `baseq3/`; **TA** → `baseq3/` + `missionpack/`;
  **OA** → `baseoa/` (gated by `STANDALONEOA`/`STANDALONETA` ifdefs).
- The Makefile bundles only the relevant fixes per flavor: it drops the other variants' splash zips, and
  Classic also drops `missionpack/pak4-ps4.pk3`. `fixes/baseq3/pak9-ps4/` is a **source tree** — only the
  built `.pk3` ships, not the loose source. The fix-file list is auto-discovered (`find fixes -type f`):
  drop a file in the folder and it's bundled.
- **The UI/OSK behaviour that runs on hardware is the compiled QVM inside `fixes/*/pak9-ps4s.pk3` (and
  `pak4-ps4.pk3` for TA), not the C in `code/q3_ui/`.** Editing C alone changes nothing on the console
  until the QVM is rebuilt and re-shipped. When OSK/menu behaviour regresses, check and rebuild the pk3
  first. (The QVM toolchain lives outside this repo in `New-stuff/qvm-scripts/`.)
- **OSK cvar contract** (`ps4_input.c` ↔ UI QVM): `ui_ime_target` (set while a field has focus, cleared
  on menu exit), `ui_ime_field` (which field the result belongs to), `ui_ime_text` (confirmed string),
  `ui_ime_done` (1 on confirm). Cross in a menu opens the field OSK for a non-empty `ui_ime_target`;
  otherwise it sends `K_ENTER`. `MenuField_Draw` covers all non-ownerdraw fields automatically.

---

## Input / controls

- **In-game:** L-stick move, R-stick look, R2 fire, L2 zoom, Cross jump, Circle crouch, Square prev-weapon,
  Triangle next-weapon, L1 strafe-left, R1 use, L3 walk/run, Touchpad scores, Options menu. Default binds
  applied by `PS4_ApplyDefaultBindings()` **after `Com_Init`** (so the config is fully loaded first).
- **Menus:** Cross/Circle = `K_ENTER`, Square/Triangle = `K_ESCAPE` — mutually exclusive with the normal
  JOY keys to prevent the double-key-event bug.
- **Touchpad aim:** L3+Touchpad toggles stick (lightbar **blue**) vs touchpad aim (lightbar **cyan**;
  swipe sends `SE_MOUSE` deltas). Cvars `ps4_aimSensX/Y` (default 0.5). Gyro aim was removed.
- **Rumble:** `ps4_rumbleEnable` (1) / `ps4_rumbleScale` (1.0), both `CVAR_ARCHIVE`. Fires only for own
  weapon fire (per-weapon strength), own pain, and hit feedback — matched by sfx name in
  `PS4_RumbleForSfx` (`snd_dma.c` → `ps4_input.c`). L3+R3 toggles enable.
- **Lightbar health (in-game):** hue-alternation, not brightness (Orbis SDK renders all RGB at full
  intensity and ignores alpha, so pulsing must change hue). 100→50 green→yellow (solid); 50→25
  yellow→red (3 s pulse, orange alt); 25→1 red→purple (1 s pulse); ≤0 purple. Health color wins over
  aim-mode color while in-game.
- **OSK:** L1+Touchpad = console command (opens console overlay; Cross opens IME, Circle closes), R1+
  Touchpad = chat (`say`). `sceImeDialogGetResult()` **must** be called before `sceImeDialogTerm()`.
  Audio is paused (`PS4_AudioPause`) while the OSK is open; on close, buttons are unstuck and input flushed.
- **Console:** R-stick Y scrolls line-by-line, D-pad Up/Down 5 lines, L1+R1 jumps top/bottom, Circle closes.

---

## Standalone variants

| Variant | `BASEGAME` | Boot extra | Data needed on PS4 |
|---|---|---|---|
| ioQuake 3 | `baseq3` | — | `/data/ioq3/baseq3/pak0–8.pk3` |
| Open Arena | `baseoa` | — | `/data/ioq3/baseoa/pak*.pk3` |
| Team Arena | `baseq3` | `+set fs_game missionpack` | baseq3 pak0–8 **+** `/data/ioq3/missionpack/pak0–3.pk3` |
| Quake 3 Classic | `baseq3` | legacy protocol 43 | baseq3 pak0–2 (stock 1.13n) + `zpack-classic.pk3` (+ `dc-mappack.pk3`) |

- TA is a **mod layered on baseq3**, not a true standalone. `BASEGAME="missionpack"` cuts baseq3 out of the
  loader and crashes `CE-34878-0` at `files.c` (`!foundPak`). The missionpack `pak1–3` must be the
  **point-release files** (Steam TA ships only `pak0`; the QVM validates checksums, so dummy paks fail).
  Source them from an ioquake3 PC install, e.g. `D:\Program Files (x86)\ioquake3\missionpack\`.

---

## CLASSIC build (Dreamcast crossplay)

`make classic` produces a Quake 3 **1.13n**–compatible client/server (`PROTOCOL_VERSION 43`,
`LEGACY_PROTOCOL`) for crossplay with Dreamcast Q3 1.13n clients/servers. **Hardware-verified 2026-06-23.**
All CLASSIC logic is behind `#ifdef CLASSIC` and must not affect Q3/OA/TA builds. Files touched (17):
`cgame/{cg_event,cg_main}.c`, `client/{cl_input,cl_main,cl_net_chan,cl_parse}.c`,
`qcommon/{files,msg,net_chan,qcommon.h}`, `server/{server.h,sv_client,sv_init,sv_main,sv_net_chan,sv_snapshot}.c`,
`ps4/sys_main_ps4.c`.

Key behaviours:
- **pak filter (`files.c`):** CLASSIC uses an explicit whitelist — `zpack-classic.pk3` + `dc-mappack.pk3`
  always pass; plain `pakN` passes only if `N ≤ 2`; any suffixed/unknown pak is skipped. The custom pak
  needs a **non-numeric suffix** (`zpack-classic`) so its checksum is excluded from the pure-checksum
  exchange (`sscanf("pak%d")` fails → skipped).
- **Pure checksums:** `FS_ReferencedPakPureChecksums(qboolean compat)` sends only pak0–2 plain CRCs (no
  serverId prefix, no trailing count) so the Dreamcast server accepts them. `clc.compat` is set when
  `com_legacyprotocol == com_protocol` (both 43). `cl_paks` command registered alongside `cp`.
- **Parse:** proto-43 has no `svc_EOF`; `CL_ParseServerMessage` downgrades the read-past-end error to a
  warning + break when `msg->compat`. `reliableAcknowledge` is read inside `CL_ParseSnapshot` in compat
  mode (not in the message header).
- **Events:** Dreamcast fires `EV_BULLET` (55) for machinegun tracers; modern cgame has no handler →
  disconnect on every MG kill. CLASSIC adds the `EV_BULLET` case in `cg_event.c`. **Lives in the QVM** —
  recompile cgame with `-DCLASSIC`.
- **CS_GAME_VERSION:** CLASSIC skips the check when the server sends an empty string (DC 1.13n doesn't set
  it). Also QVM-side (`cg_main.c`, compile with `-DCLASSIC`).
- **Master server:** `sv_master1` defaults to `dc.dreamcast-talk.com` (Internet tab); `sv_master2` empty.

The full per-port backport checklist is preserved in git history (commit `bd8509d`) if needed for the
Wii/PS3 ports.

---

## Known-incomplete items

- **Banner Z-fighting** — 16-bit FBO depth has ~256× less precision than desktop 24-bit; banners flush to
  walls Z-fight. `glPolygonOffset` is a **no-op on Piglet** (verified — no call site/factor changes it).
  Intended fix: a shader-override `pak10.pk3` adding `polygonOffset` + `sort banner` to the affected
  `base_wall.shader` entries, bundled in `fixes/baseq3/`. Pending.
- **sRGB textures** — enums defined for compile only; non-functional on GLES2. sRGB DDS would upload wrong.
  Harmless for current gameplay.
- **Shadow mapping** — depth-only FBOs unsupported on Piglet → silently no shadows, no crash.
- **Shader binary cache** — `glGetProgramBinaryOES` is NULL on FW 9.00 (§Renderer); no workaround without
  devkit access.

---

## Checklist before touching the tree

1. Read this file. (`README.md` is user-facing install/usage; consult it for end-user behaviour.)
2. Confirm the three `common.c` memory patches (§Memory model) are present.
3. If changing hunk/zone size, update `user_mem.c` try_sizes too.
4. If touching UI/OSK/menus, rebuild the QVM in `fixes/*/pak9-ps4s.pk3` (and `pak4-ps4.pk3` for TA) —
   C edits alone don't reach the hardware.
5. Keep every change behind `#ifdef __ORBIS__` (CLASSIC behaviour behind `#ifdef CLASSIC`); prefer
   `code/ps4/`.
6. Don't add OpenAL / curl / VoIP / SDL-GL / JIT — deliberately excluded.
7. No AI attribution in commits.
8. Don't build a PKG until explicitly asked.

---

*Port complete and hardware-verified across all four variants (FW 9.00, GoldHEN). Last updated 2026-06-27.*
