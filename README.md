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

These Sony proprietary modules must be obtained separately and placed in `sce_module/`:

| File |
|---|
| `libScePigletv2VSH.sprx` |
| `libSceShaccVSH.sprx` |

---

## Building

```bash
make          # Release build (no log file)
make debug    # Debug build   (writes /data/ioq3/ioquake3log.txt)
make clean    # Remove all build artifacts
```

> **Note:** Run `make clean` before switching between release and debug builds.

**Output:** `IV0000-BREW00003_00-IOQ3PS4PORT00000.pkg`

---

## PS4 directory layout

```
/app0/
└── eboot.bin                ← installed by the pkg

/data/ioq3/
├── baseq3/
│   ├── pak0.pk3             ← Quake III Arena retail data
│   ├── pak1.pk3
│   ├── ...
│   └── pak8.pk3
└── ioquake3log.txt          ← written only in debug builds

/data/self/system/common/lib/
├── libScePigletv2VSH.sprx   ← FTP here
└── libSceShaccVSH.sprx      ← FTP here
```

**You need the original Quake III Arena data files** (`pak0.pk3` through
`pak8.pk3`). On Steam these are at
`steamapps/common/Quake 3 Arena/baseq3/`.

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

## Technical notes

- **Renderer:** renderergl2 (programmable pipeline). GLES 2.0 via Piglet.
  Fixed-function GL1 calls do not exist in GLES 2, so renderergl1 is not used.
- **Shaders:** compiled at runtime via `libSceShaccVSH.sprx` as GLSL ES 1.00
  (`#version 100`). ~62 variants compile in ~16 seconds on first boot.
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
