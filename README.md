# ioquake3-PS4

A port of [ioQuake3](https://github.com/ioquake/ioq3) to the PlayStation 4, using [OpenOrbis](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain)

Five builds are produced from the same source tree:

| Variant | TITLE_ID | Base game | 
|---|---|---|---|
| ioQuake3         | `QUAK03000` | `baseq3` | 
| Team Arena       | `QUAK03001` | `baseq3` + `missionpack` | 
| Open Arena       | `QUAK03002` | `baseoa` | 
| Quake 3 Classic  | `QUAK03003` | `baseq3` (pak0–pak2 only) | 
| Elite Force      | `QUAK03004` | `baseEF` | 

## Status

- Boots, loads maps, gameplay with bots (Q3/TA/OA/Classic), online multiplayer
- Networking: LAN discovery, internet server browser, master server, hosting
- DualShock 4 dual-stick analog input + rumble, touchpad aim mode, lightbar health feedback
- On-screen keyboard support for console, chat, and menu text fields

## Prerequisites (Windows)

### 1. OpenOrbis toolchain

Follow [this guide](https://github.com/OpenPS4/guide-to-install-orbisdev) to
install the OpenOrbis PS4 toolchain, then set the `OO_PS4_TOOLCHAIN`
environment variable to wherever it was installed (path must not contain
spaces).

### 2. LLVM 18 / Clang

Install [LLVM 18](https://releases.llvm.org/) for Windows.  
The Makefile auto-detects the default install path (`C:\Program Files\LLVM\bin`).

### 3. MSYS2 make

Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) or stock
[MSYS2](https://www.msys2.org/) and ensure `make` is available.  
Build via PowerShell:

```powershell
$env:OO_PS4_TOOLCHAIN = "C:\OpenOrbis\OpenOrbis-PS4-Toolchain"
$env:PATH = "C:\devkitPro\msys2\usr\bin;" + $env:PATH
make -C "path\to\ioquake3-PS4"
```

### 4. .NET 6 or later

Required by `PkgTool.Core` (bundled in the toolchain) to generate the `.pkg`.
Install the [.NET SDK](https://dotnet.microsoft.com/download) and ensure
`dotnet` is on your PATH.

### 5. Runtime modules (not required)

Nothing to FTP here. Piglet (Sony's GLES2/EGL driver) loads automatically
from the console's own firmware, and all shaders ship precompiled in the
PKG -- the runtime shader compiler (`libSceShaccVSH.sprx`, a devkit-only
module) is never needed. Hardware-verified booting with both
`libScePigletv2VSH.sprx` and `libSceShaccVSH.sprx` absent from
`/data/self/system/common/lib/`.

## Building

Five pkg variants are produced from the same unified Makefile:

```bash
make                  # ioQuake 3            (BASEGAME=baseq3,  TITLE_ID=QUAK03000)
make ta               # Quake 3: Team Arena  (BASEGAME=baseq3 + auto fs_game=missionpack,
                      #                       TITLE_ID=QUAK03001)
make oa               # Open Arena           (BASEGAME=baseoa,  TITLE_ID=QUAK03002)
make classic          # Quake 3 Classic      (BASEGAME=baseq3,  TITLE_ID=QUAK03003,
                      #                       protocol 43 — Dreamcast crossplay)
make ef               # Elite Force          (Star Trek Voyager: Elite Force, retail QVMs)
make all-flavors      # Build all five release pkgs in sequence
make debug            # Debug build of ioQuake 3 (writes /data/ioq3/ioquake3log.txt)
make clean            # Remove all build artifacts
```

Each variant uses its own object directory (`build/obj/q3/release`,
`build/obj/ta/release`, `build/obj/oa/release`, etc.), so switching builds
never requires `make clean`. Release and debug binaries can coexist.
`make all-flavors` builds all five packages in one pass.

**Output:** `IV0000-QUAK03000_00-IOQ3PS4PORT00000.pkg` (and `QUAK03001` /
`QUAK03002`/ `QUAK03003` for TA / OA / CLASSIC).

Team Arena is a mod that layers on top of `baseq3`, not a standalone game --
the TA pkg auto-injects `+set fs_game missionpack` at boot, so both
`baseq3/` *and* `missionpack/` paks must be present on the PS4 (see the
"Mods" section below). Open Arena is a true standalone and only needs its
own `baseoa/` paks.

---

## Bundling your own files (the `fixes/` folder)

Anything placed under the `fixes/` directory in the source tree is **baked
into the PKG automatically** and copied to `/data/ioq3/` on the console's
first boot (a marker file prevents re-copying on later boots). The build
discovers files dynamically -- there is no hardcoded file list -- so you can
drop in extra paks, configs, shader overrides, music, etc. and they ship with
the next build. For example:

```
fixes/
├── baseq3/
│   ├── pak9-ps4.pk3        ← shipped UI / control patches
│   ├── zpack-classic.pk3   ← shipped UI / control patches adapted for Classic build (other builds will ignore this)
│   └── pak10.pk3           ← drop in your own override pak, it just works
├── missionpack/
│   └── pak9-ps4.pk3
├── baseoa/
│   └── 
└── shaderbin/               ← precompiled shader binaries, shared by every variant
```

Files are auto-detected anywhere under the three game folders (`baseq3/`,
`missionpack/`, `baseoa/`) and their subdirectories, as well as in the
`fixes/` root. Each variant only installs its relevant folders (Q3 →
`baseq3/`, TA → `baseq3/` + `missionpack/`, OA → `baseoa/`).

> **Adding a *new top-level* folder under `fixes/`** (e.g. a fourth game dir)
> requires a small Makefile edit -- the three base folder names are declared
> explicitly in the pkg manifest. Adding files or subfolders inside the
> existing three needs no changes.

To force a reinstall of updated fix files, delete the marker
(`/data/ioq3/fixes_installed_<variant>.txt`) on the console via FTP.

---

## PS4 directory layout

See **[INSTALLATION.md](INSTALLATION.md)** for exactly where each variant's
game data goes under `/data/ioq3/`. No Sony modules need to be FTP'd
anywhere -- see [Runtime modules](#5-runtime-modules-not-required) above.

---

## Installing on PS4

See **[INSTALLATION.md](INSTALLATION.md)** for the full step-by-step, including
where to get each build's required game files and exactly where to place them.

---

## Controls

#### In-game

| Input | Action |
|---|---|
| Left stick | Move (forward/back + strafe) |
| Right stick | Look (yaw + pitch) |
| **R2** | Fire |
| **L2** | Zoom |
| **Cross** | Jump |
| **Circle** | Crouch |
| **Square** | Previous weapon |
| **Triangle** | Next weapon |
| **L1** | Strafe left |
| **R1** | Use item |
| **L3** | Walk / run toggle |
| **Touchpad** | Scoreboard |
| **Options** | Menu (Escape) |
| **L3 + Touchpad** | Toggle stick / touchpad aim |
| **L3 + R3** (no Touchpad) | Toggle rumble on/off |

All buttons are rebindable from the in-game Controls menu.

#### Aim modes

Two aiming styles can be toggled at runtime with **L3 + Touchpad**. The
lightbar colour shows which mode is active:

| Lightbar | Mode | Look input |
|---|---|---|
| **Blue** (default) | Stick aim | Right stick |
| **Cyan** | Touchpad aim | Swipe a finger across the touchpad |

In touchpad aim, lifting your finger resets the anchor so the next touch
doesn't jump. Right-stick look speed is adjustable from the in-game Controls
menu; touchpad sensitivity uses the cvars `ps4_aimSensX` / `ps4_aimSensY`
(default `0.5` each).

The lightbar also reflects player health in-game: dim red below 50 HP and a
pulsing red below 25 HP, returning to the aim-mode colour when healthy.

#### Rumble

DualShock 4 rumble is enabled by default. It triggers only for events tied to
the local player: own weapon fire (per-weapon strength), own pain, and hit
feedback. Toggle it from the in-game Controls menu or at runtime with
**L3 + R3** (a short ack pulse plays on enable). Two cvars back it:

- `ps4_rumbleEnable` (default `1`) -- master on/off.
- `ps4_rumbleScale` (default `1.0`, range `0.0`-`1.0`) -- global intensity.

#### Player name

The default player name is taken from your PSN profile on first launch. To
change it, edit the **Name** field under Setup / Player in the in-game options
menu (the on-screen keyboard opens when you select the field).

#### Text input (on-screen keyboard)

| Combo | Action |
|---|---|
| **L1 + Touchpad** | Open keyboard → type and execute a console command |
| **R1 + Touchpad** | Open keyboard → type and send a chat message |
| **Options + Touchpad** | Toggle console overlay (view output) |

#### Menus

| Input | Action |
|---|---|
| Left stick | Move cursor |
| D-pad | Arrow keys |
| **Cross** | Confirm (Enter) |
| **Circle** | Confirm (Enter) |
| **Square** | Back (Escape) |
| **Triangle** | Back (Escape) |
| **Options** | Escape |

---

## Mods

Mod switching works on hardware: the engine tears down and rebuilds the EGL
surface and GL context cleanly when `fs_game` changes, so any mod that ships
as a `.pk3` set under `/data/ioq3/<modname>/` is playable. Launch with:

```
/set fs_game <modname>
/vid_restart
```

from the in-game console (Options + Touchpad).

#### Team Arena

Available either as a dedicated pkg (`make ta`, which auto-sets
`fs_game missionpack` at boot) or by running it as a mod from the ioQuake 3
pkg. Either way it needs the four mission-pack paks in
`/data/ioq3/missionpack/`:

```
/data/ioq3/missionpack/
├── pak0.pk3
├── pak1.pk3
├── pak2.pk3
└── pak3.pk3
```

The Steam build of Team Arena ships only `pak0`; the point-release files
are present in a normal **ioquake3 install** under `missionpack/` -- source
them there and FTP them onto the PS4. Dummy / empty paks do not work --
the QVM validates checksums.

The TA pkg keeps `BASEGAME=baseq3` and layers `missionpack` on top, so
**both** `/data/ioq3/baseq3/` and `/data/ioq3/missionpack/` must be present
even when launching from the TA pkg.

#### Open Arena

A standalone pkg (`make oa`) is provided for the free
[OpenArena](https://openarena.ws/) content. Drop the OA paks into
`/data/ioq3/baseoa/`:

```
/data/ioq3/baseoa/
├── pak0-pak6.pk3
└── ...
```

OA is a true standalone -- it does not require `baseq3/` to be present.

#### Quake 3 Classic (Dreamcast crossplay)

`make classic` builds a fourth pkg that speaks **Quake III Arena protocol 43** -- the protocol used by the original 1999 Dreamcast release. Modern ioQuake3 uses protocol 68 and is not compatible with Dreamcast servers, so this variant exists purely to enable crossplay between PS4 and the small community still running Dreamcast-era servers. The Internet server browser points at `dc.dreamcast-talk.com` out of the box.

Only `pak0`–`pak2` are loaded (byte-identical to the Dreamcast data files); higher paks and PS4-specific paks are excluded so their checksums do not interfere with the server authentication handshake.

To play on Dreamcast community servers you also need `dc-mappack.pk3`, which contains the maps in rotation on those servers. Download it from [lvlworld.com](https://lvlworld.com/download/id:999) and place it in `/data/ioq3/baseq3/`.

---

## Technical notes

- **Renderer:** renderergl2 (programmable pipeline). GLES 2.0 via Piglet.
  Fixed-function GL1 calls do not exist in GLES 2, so renderergl1 is not used.
- **Shaders:** GLSL ES 1.00 (`#version 100`) sources are compiled offline, once,
  into Piglet's native per-stage Shader Binary format and shipped in
  `fixes/shaderbin/` (124 blobs, ~1.5 MB, all four variants share one set).
  Loaded via `glShaderBinary` at boot -- no runtime compile, no ShaccVSH
  dependency. A source-compile fallback exists (`ps4_shaderbin.c`) for
  capturing newly added shader content in a debug build, but ships nothing
  extra to end users.
- **Shadow maps:** depth-only FBOs return `GL_FRAMEBUFFER_UNSUPPORTED` on
  Piglet. Shadow mapping is effectively disabled; the engine falls back to
  no-shadow rendering gracefully.
- **HDR:** forced to RGBA8 on PS4 (no `GL_RGBA16F` in GLES 2). `r_hdr` has
  no effect.
- **Audio:** DMA audio via `sceAudioOutOpen` at 48 kHz / 16-bit stereo. No
  OpenAL dependency.
- **Networking:** BSD sockets via `libxnet.a` (POSIX layer over `sceNet`).
  IPv4 only. DNS resolution via `sceNetResolverStartNtoa`. Direct IP connect
  works; name resolution may not resolve all hosts.
- **QVM:** interpreter mode only (no JIT). `vm_interpreted.c` handles all
  game/UI/cgame bytecode. `vm_game`, `vm_cgame`, and `vm_ui` are forced to
  interpreter (`1`) at VM init, overriding any stale value in `q3config.cfg`.
- **sv_pure:** forced to `0` on PS4. With `sv_pure 1` the server resends the
  gamestate on every client usercmd until pak checksums are authenticated; PS4's
  several-second cgame load causes an infinite "Awaiting Snapshot" loop. The
  override happens at `SV_Init` level because `sv_pure` is `CVAR_SYSTEMINFO`
  (broadcast by the server in the gamestate packet, overriding any command-line
  `+set`).
- **Memory:** Piglet configured for 250 MB system + 512 MB video + 170 MB
  flex. Engine hunk is 256 MB.
- **Log file:** written to `/data/ioq3/ioquake3log.txt` only in debug builds
  (`make debug` / `make DEBUG=1`). Release builds produce no log file.

---

## Credits

- **[ioQuake3](https://github.com/ioquake/ioq3)** -- the upstream engine this port is based on.
- **[OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain)** -- LLVM-based toolchain, ELF linker, and PkgTool used to produce PS4 pkgs.
- **[psbc](https://gitgud.io/veiledmerc/psbc)** -- Shader compilation
- **[Lilium Arena Classic](https://github.com/clover-moe/lilium-arena-classic)** (clover-moe / clover-leaf) -- reverse-engineered Quake III Arena protocol-43 / Dreamcast compatibility layer. The CLASSIC build's pure-checksum exchange, `cl_paks` format, server-message parse fixes, and `FS_ReferencedPakPureChecksums` compat mode are derived from this work.

---

## AI disclosure

Parts of this port were developed with the assistance of **Claude** (Anthropic). AI was used for code generation, debugging, porting guidance, and documentation. All AI-generated code was reviewed and tested on hardware before inclusion.

---

## License

ioQuake3 is GPLv2. This PS4 port layer is also GPLv2. See `LICENSE`.
