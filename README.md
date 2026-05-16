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

## Building

Three pkg variants are produced from the same source tree:

```bash
make                  # ioQuake 3        (BASEGAME=baseq3,  TITLE_ID=QUAK03000)
make -f Makefile.oa   # Open Arena       (BASEGAME=baseoa,  TITLE_ID=QUAK03002)
make -f Makefile.ta   # Team Arena       (BASEGAME=baseq3 + auto fs_game=missionpack,
                      #                   TITLE_ID=QUAK03001)
make debug            # Debug build of ioQuake 3 (writes /data/ioq3/ioquake3log.txt)
make clean            # Remove all build artifacts
```

Each variant uses its own object directory (`build/obj/release[/oa/ta]`,
`build/obj/debug`), so switching builds never requires `make clean`. Release
and debug binaries can coexist.

> **Build one variant at a time.** Chaining all three back-to-back has
> produced broken pkgs on hardware. Always fully clean
> (`rm -rf build eboot.bin pkg.gp4 sce_sys/param.sfo IV0000-QUAK*.pkg`)
> between variants and install/test each one before moving on.

**Output:** `IV0000-QUAK03000_00-IOQ3PS4PORT00000.pkg` (and `QUAK03001` /
`QUAK03002` for TA / OA).

Team Arena is a mod that layers on top of `baseq3`, not a standalone game --
the TA pkg auto-injects `+set fs_game missionpack` at boot, so both
`baseq3/` *and* `missionpack/` paks must be present on the PS4 (see the
"Mods" section below). Open Arena is a true standalone and only needs its
own `baseoa/` paks.

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
**You'll also need to extract the OpenGL module libScePigletv2VSH.sprx and the shader compiler module libSceShaccVSH.sprx from RetroArch_PS4_r4.pkg. You can search and find this package online.**
```
/data/self/system/common/lib/
├── libScePigletv2VSH.sprx   ← FTP here
└── libSceShaccVSH.sprx      ← FTP here
```

---

## Controls

Dual-stick FPS layout. Buttons are rebindable from the in-game options menu.

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
| **R1** | Strafe right |
| **L3** | Walk / run toggle |
| **R3** | Scoreboard |
| **Touchpad** | Scoreboard |
| **Options** | Menu (Escape) |
| **L3 + Touchpad** | Toggle touchpad aim |
| **R3 + Touchpad** | Toggle gyro aim *(experimental)* |
| **L3 + R3** (no Touchpad) | Toggle rumble on/off |

#### Aim modes

Two alternative aiming styles can be toggled at runtime via touchpad combos.
The lightbar colour shows which mode is active:

| Lightbar | Mode | Look input |
|---|---|---|
| **Blue** (default) | Stick aim | Right stick |
| **Cyan** | Touchpad aim | Swipe a finger across the touchpad |
| **Green** | Gyro aim *(experimental)* | Tilt the controller (DualShock 4 IMU) |

In touchpad aim, lifting your finger resets the anchor so the next touch
doesn't jump. Sensitivity is set via:

- Touchpad: cvars `ps4_aimSensX` / `ps4_aimSensY` (default `0.5` each).
- Gyro: cvars `ps4_gyroSensYaw` / `ps4_gyroSensPitch` (default `5.0` each).

All four are archived to `q3config.cfg`.

> **Gyro aim is experimental.** It works but still needs polish: the response
> curve, default sensitivities, and dead-zone behavior may not feel right yet,
> and it has not been tuned against competitive map flow. Feedback welcome.

#### Rumble

DualShock 4 rumble is enabled by default. It triggers only for events tied to
the local player: own weapon fire (per-weapon strength), own pain, and hit
feedback. Toggle on/off at runtime with **L3 + R3** (a short ack pulse plays
on enable). Configurable via:

- `ps4_rumbleEnable` (default `1`) -- master on/off, archived to `q3config.cfg`.
- `ps4_rumbleScale` (default `1.0`, range `0.0`-`1.0`) -- global intensity.

#### Player name

The default player name is taken from your PSN profile on first launch
(via `sceUserServiceGetUserName`). To override, change it from `q3config.cfg`.

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
| **Circle** | Back (Escape) |
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

Available either as a dedicated pkg (`make -f Makefile.ta`, which auto-sets
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

A standalone pkg (`make -f Makefile.oa`) is provided for the free
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
  This follows the approach taken by RetroArch's PS4 build: the Piglet GLES2
  runtime and the ShaccVSH compiler module are loaded from
  `/data/self/system/common/lib/` -- both modules are extracted from
  `RetroArch_PS4_r4.pkg` and dropped into place via FTP. They are devkit-only
  components that are not present on retail FW 9.00 out of the box.
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
  game/UI/cgame bytecode.
- **Memory:** Piglet configured for 250 MB system + 512 MB video + 170 MB
  flex. Engine hunk is 256 MB.
- **Log file:** written to `/data/ioq3/ioquake3log.txt` only in debug builds
  (`make debug` / `make DEBUG=1`). Release builds produce no log file.

---

## License

ioQuake3 is GPLv2. This PS4 port layer is also GPLv2. See `LICENSE`.
