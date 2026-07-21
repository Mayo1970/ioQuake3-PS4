# Installation

This port produces **five separate PS4 packages** from one source tree. Each
is a standalone game/EBOOT with its own TITLE_ID — install only the ones you
want. None of them include the copyrighted game data; you provide your own
`.pk3` files, bought legally, and FTP them to the console.

## What each build is

| Build | What it is | Notable features |
|---|---|---|
| **ioQuake3** | Quake III Arena | Full gameplay, bots, online/LAN multiplayer, mods via `fs_game` |
| **Team Arena** | Q3A + the *Team Arena* mission pack | Adds TA's extra weapons/vehicles/game modes on top of ioQuake3 |
| **Open Arena** | Free/open Q3A-compatible game | Same engine features, uses free OA game data — no retail purchase needed |
| **Quake 3 Classic** | Q3A client speaking the 1.16n protocol| Crossplay with Sega Dreamcast community servers |
| **Elite Force** | Star Trek Voyager: Elite Force | Holomatch client|

## Where to get the required files

Buy the base game(s) legally, then copy the `.pk3` files from your
install/disc into the paths below.

- **Quake III Arena** (needed for ioQuake3, Team Arena, and Quake 3 Classic): [Here](https://www.gog.com/en/game/quake_iii_arena)
- **Open Arena** [Here](https://openarena.ws/)
- **Star Trek Voyager: Elite Force** (needed for Elite Force only): [Here](https://www.gog.com/en/game/star_trek_voyager_elite_force)
- **Dreamcast community map pack**: [Here](https://lvlworld.com/download/id:999)

---

## Common layout

All builds FTP their EBOOT to their own TITLE_ID slot, but every build reads
game data from one shared root, `/data/ioq3/`, so reinstalling a PKG never
wipes your paks or config.

```
/data/ioq3/<game dir(s) below>
```

Create `/data/ioq3/` via FTP before first boot.

---

## ioQuake3 (Q3A)

TITLE_ID: `QUAK03000`

Copy your Quake III Arena `baseq3` paks:

```
/data/ioq3/baseq3/pak0.pk3 … pak8.pk3
```

## Team Arena

TITLE_ID: `QUAK03001`

Needs **both** Q3A's `baseq3` and Team Arena's `missionpack`:

```
/data/ioq3/baseq3/pak0.pk3 … pak8.pk3
/data/ioq3/missionpack/pak0.pk3 … pak3.pk3
```
## Open Arena

TITLE_ID: `QUAK03002`

Copy Open Arena's paks — no `baseq3` needed:

```
/data/ioq3/baseoa/pak*.pk3 …
```

## Quake 3 Classic (Dreamcast crossplay)

TITLE_ID: `QUAK03003`

Only `pak0`–`pak2` are loaded (byte-identical to the Dreamcast data files);
higher paks are ignored:

```
/data/ioq3/baseq3/pak0.pk3
/data/ioq3/baseq3/pak1.pk3
/data/ioq3/baseq3/pak2.pk3
```

To play on Dreamcast servers, also add the community map pack:

```
/data/ioq3/baseq3/dc-mappack.pk3
```

## Elite Force

TITLE_ID: `QUAK03004`

Copy your Star Trek Voyager: Elite Force retail `baseEF` paks:

```
/data/ioq3/baseEF/pak0.pk3 …
```

## Installing a PKG

Every [release](https://github.com/Mayo1970/ioQuake3-PS4/releases) already
includes a prebuilt `.pkg` for each build — no need to compile anything
yourself.

1. Download the `.pkg` for the build you want from the release.
2. Install it on your jailbroken PS4 (e.g. via a PKG installer homebrew).
3. FTP the matching game data from the sections above into `/data/ioq3/`.
4. Launch from the home screen.
