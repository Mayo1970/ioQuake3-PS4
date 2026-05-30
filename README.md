# ioquake3-PS4

A port of [ioQuake3](https://github.com/ioquake/ioq3) to the PlayStation 4,
using the [OpenOrbis](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain)
homebrew toolchain and Piglet (Sony's built-in GLES 2.0 / EGL 1.4) for rendering.

## Prerequisites (Windows)

### 1. OpenOrbis toolchain

Download and extract the [OpenOrbis PS4 toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases)
to a directory without spaces in the path, e.g.:

```
C:\OpenOrbis\OpenOrbis-PS4-Toolchain\
```

Set the `OO_PS4_TOOLCHAIN` environment variable to that path.

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

### 5. Runtime modules (required, not included)

The GLES2 driver (`libScePigletv2VSH.sprx`) and the runtime shader compiler
(`libSceShaccVSH.sprx`) are devkit-only Sony modules not present on retail
firmware. You must supply both yourself and FTP them to
`/data/self/system/common/lib/` on the console. See the
[PS4 directory layout](#ps4-directory-layout) section below.

## Building

Three pkg variants are produced from the same unified Makefile:

```bash
make                  # ioQuake 3            (BASEGAME=baseq3,  TITLE_ID=QUAK03000)
make ta               # Quake 3: Team Arena  (BASEGAME=baseq3 + auto fs_game=missionpack,
                      #                       TITLE_ID=QUAK03001)
make oa               # Open Arena           (BASEGAME=baseoa,  TITLE_ID=QUAK03002)
make all-flavors      # Build all three release pkgs in sequence
make debug            # Debug build of ioQuake 3 (writes /data/ioq3/ioquake3log.txt)
make clean            # Remove all build artifacts
```

Each variant uses its own object directory (`build/obj/q3/release`,
`build/obj/ta/release`, `build/obj/oa/release`, etc.), so switching builds
never requires `make clean`. Release and debug binaries can coexist.
`make all-flavors` builds all three packages in one pass.

**Output:** `IV0000-QUAK03000_00-IOQ3PS4PORT00000.pkg` (and `QUAK03001` /
`QUAK03002` for TA / OA).

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
│   ├── pak9-ps4s.pk3        ← shipped UI / control patches
│   └── pak10.pk3            ← drop in your own override pak, it just works
├── missionpack/
│   └── pak9-ps4s.pk3
├── baseoa/
│   └── pak9-ps4s.pk3
├── splash.zip               ← boot splash (Quake 3)
├── ta.zip                   ← boot splash (Team Arena)
└── oa.zip                   ← boot splash (Open Arena)
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

**You need the original Quake III Arena data files** (`pak0.pk3` through
`pak8.pk3`). On Steam these are at
`steamapps/common/Quake 3 Arena/baseq3/`

```
/data/ioq3/
├── baseq3/
│   ├── pak0.pk3             ← Quake III Arena retail data
│   ├── pak1.pk3
│   ├── ...
│   └── pak8.pk3
├── missionpack/             ← optional, for Team Arena (see "Mods" below)
│   ├── pak0.pk3
│   ├── pak1.pk3
│   ├── pak2.pk3
│   └── pak3.pk3
└── ioquake3log.txt          ← written only in debug builds
```
**You'll also need the OpenGL module `libScePigletv2VSH.sprx` and the shader compiler module `libSceShaccVSH.sprx`. These are devkit-only Sony modules not present on retail firmware; supply your own and FTP them into place.**
```
/data/self/system/common/lib/
├── libScePigletv2VSH.sprx   ← FTP here
└── libSceShaccVSH.sprx      ← FTP here
```

---

## Controls

Dual-stick FPS layout.

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

---

## Technical notes

- **Renderer:** renderergl2 (programmable pipeline). GLES 2.0 via Piglet.
  Fixed-function GL1 calls do not exist in GLES 2, so renderergl1 is not used.
- **Shaders:** compiled at runtime as GLSL ES 1.00 (`#version 100`) via
  `libSceShaccVSH.sprx`. ~62 variants compile in ~16 seconds on first boot.
  The Piglet GLES2 runtime and the ShaccVSH compiler module are loaded from
  `/data/self/system/common/lib/` -- both are devkit-only components not
  present on retail FW 9.00 out of the box, so they must be supplied and FTP'd
  into place (see [Runtime modules](#5-runtime-modules-required-not-included)).
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

## License

ioQuake3 is GPLv2. This PS4 port layer is also GPLv2. See `LICENSE`.
