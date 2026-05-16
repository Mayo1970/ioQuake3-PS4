# ============================================================================
# ioQuake3 PS4 Port - Makefile
#
# Based on the OpenOrbis sample Makefile pattern.
# Builds ioQuake3 for PS4 using Piglet (GLES2/EGL) for rendering.
# Also builds QVM files for baseta game modules.
# ============================================================================

# ============================================================================
# Package metadata — MUST be before targets that use them
# ============================================================================

TITLE       := ioQuake 3
VERSION     := 1.00
TITLE_ID    := QUAK03000
CONTENT_ID  := IV0000-QUAK03000_00-IOQ3PS4PORT00000

# ============================================================================
# Default target MUST be first - before any file targets
# ============================================================================

.PHONY: all release debug clean qvms icon

all: release

release: $(CONTENT_ID).pkg

debug:
	$(MAKE) DEBUG=1 $(CONTENT_ID).pkg

# ============================================================================
# OpenOrbis toolchain root (set OO_PS4_TOOLCHAIN environment variable)
# Convert backslashes to forward slashes for MSYS2/bash compatibility
# ============================================================================

TOOLCHAIN   := $(subst \,/,$(OO_PS4_TOOLCHAIN))

# Debug build support
#   make           -- release build (no log file written at runtime)
#   make DEBUG=1   -- debug build   (writes /data/ioq3/ioquake3log.txt)
# Separate object directories mean switching modes never needs 'make clean'.
DEBUG ?= 0

# Output directories
OUTDIR      := build
ifeq ($(DEBUG),1)
    INTDIR  := build/obj/debug
else
    INTDIR  := build/obj/release
endif

# ============================================================================
# Icon copy
# ============================================================================

ICON_SRC = icons/q3/icon0.png
ICON_DST = sce_sys/icon0.png

icon: $(ICON_SRC)
	@mkdir -p $(dir $(ICON_DST))
	cp $< $(ICON_DST)

# ============================================================================
# Compiler / Linker selection
# ============================================================================

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(UNAME_S),Linux)
    CC      := clang
    CXX     := clang++
    LD      := ld.lld
    CDIR    := linux
endif
ifeq ($(UNAME_S),Darwin)
    CC      := /usr/local/opt/llvm/bin/clang
    CXX     := /usr/local/opt/llvm/bin/clang++
    LD      := /usr/local/opt/llvm/bin/ld.lld
    CDIR    := macos
endif
# Windows (MSYS2, Git Bash, or similar)
# MSYS2 uname returns "MSYS_NT-*", Git Bash returns "MINGW64_NT-*", etc.
ifneq (,$(findstring NT,$(UNAME_S)))
    CDIR    := windows
    # If clang is not on PATH, inject the default Windows LLVM install location.
    # The space in "Program Files" is fine as a colon-delimited PATH component.
    ifeq ($(shell which clang 2>/dev/null),)
        _LLVM_TRY := $(shell ls -d "/c/Program Files/LLVM/bin" 2>/dev/null ||                               ls -d "/c/LLVM/bin" 2>/dev/null || echo "")
        ifneq ($(_LLVM_TRY),)
            export PATH := $(_LLVM_TRY):$(PATH)
        endif
    endif
    CC      := clang
    CXX     := clang++
    LD      := ld.lld
endif
# Also handle MINGW/MSYS environment strings
ifneq (,$(findstring MINGW,$(UNAME_S)))
    CC      := clang
    CXX     := clang++
    LD      := ld.lld
    CDIR    := windows
endif
ifneq (,$(findstring MSYS,$(UNAME_S)))
    CC      := clang
    CXX     := clang++
    LD      := ld.lld
    CDIR    := windows
endif

# ============================================================================
# QVM Toolchain (for building game module bytecode)
# ============================================================================
# QVMs are built using the host PC compiler, NOT the PS4 cross-compiler.
# These tools are in the ioquake3 repo under code/tools/.
# If not available, set Q3LCC/Q3ASM to the path where you built them.
#
# To build the QVM tools from ioquake3 source:
#   cd code/tools && build_qvm_tools.bat  (on Windows with VS)
#
# The QVM tools run on your PC and output .qvm bytecode files that the
# PS4 engine interprets at runtime.
# ============================================================================

Q3LCC := code/tools/bin/q3lcc
Q3ASM := code/tools/bin/q3asm

# QVM output directory
QVM_DIR := build/vm

# ============================================================================
# PS4 compilation flags
# ============================================================================

# Target: x86_64 PS4 ELF (Sony SCE PS4 triple)
# SM64 PS4 port uses this triple and it works on retail FW 9.00.
# The FreeBSD triple (x86_64-pc-freebsd12-elf) produces binaries where
# eglGetDisplay fails -- possibly due to different dynamic symbol resolution.
TARGET      := --target=x86_64-scei-ps4-elf

# PS4 platform defines
# NOTE: Features disabled by NOT defining them (not =0), because the
# codebase uses #ifdef (presence check), not #if (value check).
DEFINES     := -D__ORBIS__ -D__PS4__ \
               -DBOTLIB \
               -DUSE_INTERNAL_JPEG=1 \
               -DUSE_INTERNAL_ZLIB=1 \
               -DUSE_CODEC_VORBIS=1 \
               -DPRODUCT_VERSION=\"1.36_ps4\"

ifeq ($(DEBUG),1)
    DEFINES += -DPS4_DEBUG
endif

CFLAGS      := $(TARGET) -fPIC -funwind-tables -fexceptions \
               -O2 -g \
               -isysroot $(TOOLCHAIN) \
               -isystem $(TOOLCHAIN)/include \
               $(DEFINES) \
               -Icode \
               -Icode/ps4 \
               -Icode/thirdparty/jpeg-9f \
               -Icode/thirdparty/zlib-1.3.1 \
               -Icode/thirdparty/libogg-1.3.6/include \
               -Icode/thirdparty/libvorbis-1.3.7/include \
               -c

# Suppress warnings that will be noisy during initial port
CFLAGS      += -Wno-unused-variable -Wno-unused-function \
               -Wno-incompatible-pointer-types -Wno-int-conversion

# crt1.o placed before -L/-l (SM64 link order: objects, crt1.o, then libs)
LDFLAGS     := -m elf_x86_64 -pie \
               --script $(TOOLCHAIN)/link.x \
               --eh-frame-hdr \
               ps4_crt1.o \
               -L$(TOOLCHAIN)/lib

# ============================================================================
# Libraries
# ============================================================================

# Core PS4 libraries
# IMPORTANT: Using -lSceLibcInternal -lScePosix instead of -lc -lc++
# SM64 PS4 port (which works on retail FW 9.00) uses Sony's internal libc,
# NOT the OpenOrbis musl-based libc.prx. Piglet internally depends on
# SceLibcInternal; using musl causes eglGetDisplay to fail silently.
# This also means libc.prx must NOT be in sce_module/.
LIBS        := -lkernel -lSceLibcInternal -lScePosix \
               -lScePigletv2VSH \
               -lSceSysmodule \
               -lSceSystemService \
               -lSceRtc \
               -lScePad \
               -lSceUserService \
               -lSceAudioOut \
               -lxnet \
               -lSceNet \
               -lSceNetCtl \
               -lSceVideoOut \
               -lSceCommonDialog \
               -lSceMsgDialog \
               -lSceSaveData \
               -lSceImeDialog

# ============================================================================
# Source files
# ============================================================================

# PS4 platform layer (replaces sdl/ and sys/)
PS4_SRCS :=     code/ps4/sys_main_ps4.c \
                code/ps4/sys_ps4.c \
                code/ps4/ps4_glimp.c \
                code/ps4/ps4_input.c \
                code/ps4/ps4_snd.c \
                code/ps4/ps4_gamma.c \
                code/ps4/net_ps4.c \
                code/ps4/con_ps4.c \
                code/ps4/ps4_compat.c \
                code/ps4/user_mem.c

# Renderer GL2 (programmable pipeline, GLES2 compatible)
RENDERER_SRCS :=     code/renderergl2/tr_animation.c \
                     code/renderergl2/tr_backend.c \
                     code/renderergl2/tr_bsp.c \
                     code/renderergl2/tr_cmds.c \
                     code/renderergl2/tr_curve.c \
                     code/renderergl2/tr_dsa.c \
                     code/renderergl2/tr_extensions.c \
                     code/renderergl2/tr_extramath.c \
                     code/renderergl2/tr_fbo.c \
                     code/renderergl2/tr_flares.c \
                     code/renderergl2/tr_glsl.c \
                     code/renderergl2/tr_image.c \
                     code/renderergl2/tr_image_dds.c \
                     code/renderergl2/tr_init.c \
                     code/renderergl2/tr_light.c \
                     code/renderergl2/tr_main.c \
                     code/renderergl2/tr_marks.c \
                     code/renderergl2/tr_mesh.c \
                     code/renderergl2/tr_model.c \
                     code/renderergl2/tr_model_iqm.c \
                     code/renderergl2/tr_postprocess.c \
                     code/renderergl2/tr_scene.c \
                     code/renderergl2/tr_shade.c \
                     code/renderergl2/tr_shade_calc.c \
                     code/renderergl2/tr_shader.c \
                     code/renderergl2/tr_shadows.c \
                     code/renderergl2/tr_sky.c \
                     code/renderergl2/tr_surface.c \
                     code/renderergl2/tr_vbo.c \
                     code/renderergl2/tr_world.c

# Renderer common
RENDERERCOMMON_SRCS :=     code/renderercommon/puff.c \
                           code/renderercommon/tr_font.c \
                           code/renderercommon/tr_image_bmp.c \
                           code/renderercommon/tr_image_jpg.c \
                           code/renderercommon/tr_image_pcx.c \
                           code/renderercommon/tr_image_png.c \
                           code/renderercommon/tr_image_pvr.c \
                           code/renderercommon/tr_image_tga.c \
                           code/renderercommon/tr_noise.c

# Client
CLIENT_SRCS :=     code/client/cl_avi.c \
                   code/client/cl_cgame.c \
                   code/client/cl_cin.c \
                   code/client/cl_console.c \
                   code/client/cl_input.c \
                   code/client/cl_keys.c \
                   code/client/cl_main.c \
                   code/client/cl_net_chan.c \
                   code/client/cl_parse.c \
                   code/client/cl_scrn.c \
                   code/client/cl_ui.c \
                   code/client/snd_adpcm.c \
                   code/client/snd_codec.c \
                   code/client/snd_codec_wav.c \
                   code/client/snd_codec_ogg.c \
                   code/client/snd_dma.c \
                   code/client/snd_main.c \
                   code/client/snd_mem.c \
                   code/client/snd_mix.c \
                   code/client/snd_wavelet.c

# Common engine
QCOMMON_SRCS :=     code/qcommon/cm_load.c \
                    code/qcommon/cm_patch.c \
                    code/qcommon/cm_polylib.c \
                    code/qcommon/cm_test.c \
                    code/qcommon/cm_trace.c \
                    code/qcommon/cmd.c \
                    code/qcommon/common.c \
                    code/qcommon/cvar.c \
                    code/qcommon/files.c \
                    code/qcommon/huffman.c \
                    code/qcommon/md4.c \
                    code/qcommon/md5.c \
                    code/qcommon/msg.c \
                    code/qcommon/net_chan.c \
                    code/qcommon/net_ip.c \
                    code/qcommon/q_math.c \
                    code/qcommon/q_shared.c \
                    code/qcommon/ioapi.c \
                    code/qcommon/unzip.c \
                    code/qcommon/vm.c \
                    code/qcommon/vm_interpreted.c \
                    code/qcommon/vm_none.c

# Server
SERVER_SRCS :=     code/server/sv_bot.c \
                   code/server/sv_ccmds.c \
                   code/server/sv_client.c \
                   code/server/sv_game.c \
                   code/server/sv_init.c \
                   code/server/sv_main.c \
                   code/server/sv_net_chan.c \
                   code/server/sv_snapshot.c \
                   code/server/sv_world.c

# Bot library
BOTLIB_SRCS :=     code/botlib/be_aas_bspq3.c \
                   code/botlib/be_aas_cluster.c \
                   code/botlib/be_aas_debug.c \
                   code/botlib/be_aas_entity.c \
                   code/botlib/be_aas_file.c \
                   code/botlib/be_aas_main.c \
                   code/botlib/be_aas_move.c \
                   code/botlib/be_aas_optimize.c \
                   code/botlib/be_aas_reach.c \
                   code/botlib/be_aas_route.c \
                   code/botlib/be_aas_routealt.c \
                   code/botlib/be_aas_sample.c \
                   code/botlib/be_ai_char.c \
                   code/botlib/be_ai_chat.c \
                   code/botlib/be_ai_gen.c \
                   code/botlib/be_ai_goal.c \
                   code/botlib/be_ai_move.c \
                   code/botlib/be_ai_weap.c \
                   code/botlib/be_ai_weight.c \
                   code/botlib/be_ea.c \
                   code/botlib/be_interface.c \
                   code/botlib/l_crc.c \
                   code/botlib/l_libvar.c \
                   code/botlib/l_log.c \
                   code/botlib/l_memory.c \
                   code/botlib/l_precomp.c \
                   code/botlib/l_script.c \
                   code/botlib/l_struct.c

# Third-party (bundled) - Updated with correct Ogg/Vorbis paths
# Third-party (bundled) - Updated with correct Ogg/Vorbis paths
THIRDPARTY_SRCS :=     code/thirdparty/zlib-1.3.1/adler32.c \
                       code/thirdparty/zlib-1.3.1/crc32.c \
                       code/thirdparty/zlib-1.3.1/inffast.c \
                       code/thirdparty/zlib-1.3.1/inflate.c \
                       code/thirdparty/zlib-1.3.1/inftrees.c \
                       code/thirdparty/zlib-1.3.1/zutil.c \
                       code/thirdparty/jpeg-9f/jaricom.c \
                       code/thirdparty/jpeg-9f/jcapimin.c \
                       code/thirdparty/jpeg-9f/jcapistd.c \
                       code/thirdparty/jpeg-9f/jcarith.c \
                       code/thirdparty/jpeg-9f/jccoefct.c \
                       code/thirdparty/jpeg-9f/jccolor.c \
                       code/thirdparty/jpeg-9f/jcdctmgr.c \
                       code/thirdparty/jpeg-9f/jchuff.c \
                       code/thirdparty/jpeg-9f/jcinit.c \
                       code/thirdparty/jpeg-9f/jcmainct.c \
                       code/thirdparty/jpeg-9f/jcmarker.c \
                       code/thirdparty/jpeg-9f/jcmaster.c \
                       code/thirdparty/jpeg-9f/jcomapi.c \
                       code/thirdparty/jpeg-9f/jcparam.c \
                       code/thirdparty/jpeg-9f/jcprepct.c \
                       code/thirdparty/jpeg-9f/jcsample.c \
                       code/thirdparty/jpeg-9f/jctrans.c \
                       code/thirdparty/jpeg-9f/jdapimin.c \
                       code/thirdparty/jpeg-9f/jdapistd.c \
                       code/thirdparty/jpeg-9f/jdarith.c \
                       code/thirdparty/jpeg-9f/jdatadst.c \
                       code/thirdparty/jpeg-9f/jdatasrc.c \
                       code/thirdparty/jpeg-9f/jdcoefct.c \
                       code/thirdparty/jpeg-9f/jdcolor.c \
                       code/thirdparty/jpeg-9f/jddctmgr.c \
                       code/thirdparty/jpeg-9f/jdhuff.c \
                       code/thirdparty/jpeg-9f/jdinput.c \
                       code/thirdparty/jpeg-9f/jdmainct.c \
                       code/thirdparty/jpeg-9f/jdmarker.c \
                       code/thirdparty/jpeg-9f/jdmaster.c \
                       code/thirdparty/jpeg-9f/jdmerge.c \
                       code/thirdparty/jpeg-9f/jdpostct.c \
                       code/thirdparty/jpeg-9f/jdsample.c \
                       code/thirdparty/jpeg-9f/jdtrans.c \
                       code/thirdparty/jpeg-9f/jerror.c \
                       code/thirdparty/jpeg-9f/jfdctflt.c \
                       code/thirdparty/jpeg-9f/jfdctfst.c \
                       code/thirdparty/jpeg-9f/jfdctint.c \
                       code/thirdparty/jpeg-9f/jidctflt.c \
                       code/thirdparty/jpeg-9f/jidctfst.c \
                       code/thirdparty/jpeg-9f/jidctint.c \
                       code/thirdparty/jpeg-9f/jmemmgr.c \
                       code/thirdparty/jpeg-9f/jmemnobs.c \
                       code/thirdparty/jpeg-9f/jquant1.c \
                       code/thirdparty/jpeg-9f/jquant2.c \
                       code/thirdparty/jpeg-9f/jutils.c \
                       code/thirdparty/libogg-1.3.6/src/bitwise.c \
                       code/thirdparty/libogg-1.3.6/src/framing.c \
                       code/thirdparty/libvorbis-1.3.7/lib/analysis.c \
                       code/thirdparty/libvorbis-1.3.7/lib/bitrate.c \
                       code/thirdparty/libvorbis-1.3.7/lib/block.c \
                       code/thirdparty/libvorbis-1.3.7/lib/codebook.c \
                       code/thirdparty/libvorbis-1.3.7/lib/envelope.c \
                       code/thirdparty/libvorbis-1.3.7/lib/floor0.c \
                       code/thirdparty/libvorbis-1.3.7/lib/floor1.c \
                       code/thirdparty/libvorbis-1.3.7/lib/info.c \
                       code/thirdparty/libvorbis-1.3.7/lib/lookup.c \
                       code/thirdparty/libvorbis-1.3.7/lib/lpc.c \
                       code/thirdparty/libvorbis-1.3.7/lib/lsp.c \
                       code/thirdparty/libvorbis-1.3.7/lib/mapping0.c \
                       code/thirdparty/libvorbis-1.3.7/lib/mdct.c \
                       code/thirdparty/libvorbis-1.3.7/lib/psy.c \
                       code/thirdparty/libvorbis-1.3.7/lib/registry.c \
                       code/thirdparty/libvorbis-1.3.7/lib/res0.c \
                       code/thirdparty/libvorbis-1.3.7/lib/sharedbook.c \
                       code/thirdparty/libvorbis-1.3.7/lib/smallft.c \
                       code/thirdparty/libvorbis-1.3.7/lib/synthesis.c \
                       code/thirdparty/libvorbis-1.3.7/lib/vorbisfile.c \
                       code/thirdparty/libvorbis-1.3.7/lib/window.c

# Architecture-specific
ASM_SRCS :=     code/asm/snapvector.c \
                code/asm/ftola.c

# GLSL stringified shaders (pre-generated in build/glsl/)
GLSL_NAMES :=     bokeh_fp bokeh_vp \
                  calclevels4x_fp calclevels4x_vp \
                  depthblur_fp depthblur_vp \
                  dlight_fp dlight_vp \
                  down4x_fp down4x_vp \
                  fogpass_fp fogpass_vp \
                  generic_fp generic_vp \
                  greyscale_fp greyscale_vp \
                  lightall_fp lightall_vp \
                  pshadow_fp pshadow_vp \
                  shadowfill_fp shadowfill_vp \
                  shadowmask_fp shadowmask_vp \
                  ssao_fp ssao_vp \
                  texturecolor_fp texturecolor_vp \
                  tonemap_fp tonemap_vp

GLSL_SRCS := $(patsubst %, build/glsl/%.c, $(GLSL_NAMES))
GLSL_OBJS := $(patsubst %, $(INTDIR)/glsl/%.o, $(GLSL_NAMES))

# All engine sources
ALL_SRCS := $(PS4_SRCS) $(RENDERER_SRCS) $(RENDERERCOMMON_SRCS) \
            $(CLIENT_SRCS) $(QCOMMON_SRCS) $(SERVER_SRCS) \
            $(BOTLIB_SRCS) $(THIRDPARTY_SRCS) $(ASM_SRCS)

# Object files
ALL_OBJS := $(patsubst code/%.c, $(INTDIR)/%.o, $(ALL_SRCS)) $(GLSL_OBJS)

# ============================================================================
# QVM Game Module Sources (if you need them)
# ============================================================================

# CGAME module sources
CGAME_QVM_SRCS :=     code/cgame/cg_main.c \
                      code/cgame/cg_consolecmds.c \
                      code/cgame/cg_draw.c \
                      code/cgame/cg_drawtools.c \
                      code/cgame/cg_effects.c \
                      code/cgame/cg_ents.c \
                      code/cgame/cg_event.c \
                      code/cgame/cg_info.c \
                      code/cgame/cg_localents.c \
                      code/cgame/cg_marks.c \
                      code/cgame/cg_particles.c \
                      code/cgame/cg_players.c \
                      code/cgame/cg_playerstate.c \
                      code/cgame/cg_predict.c \
                      code/cgame/cg_scoreboard.c \
                      code/cgame/cg_servercmds.c \
                      code/cgame/cg_snapshot.c \
                      code/cgame/cg_view.c \
                      code/cgame/cg_weapons.c \
                      code/game/bg_misc.c \
                      code/game/bg_pmove.c \
                      code/game/bg_slidemove.c \
                      code/game/bg_lib.c

# QAGAME module sources
QAGAME_QVM_SRCS :=    code/game/g_main.c \
                      code/game/g_active.c \
                      code/game/g_arenas.c \
                      code/game/g_bot.c \
                      code/game/g_client.c \
                      code/game/g_cmds.c \
                      code/game/g_combat.c \
                      code/game/g_items.c \
                      code/game/g_mem.c \
                      code/game/g_misc.c \
                      code/game/g_missile.c \
                      code/game/g_mover.c \
                      code/game/g_session.c \
                      code/game/g_spawn.c \
                      code/game/g_svcmds.c \
                      code/game/g_target.c \
                      code/game/g_team.c \
                      code/game/g_trigger.c \
                      code/game/g_utils.c \
                      code/game/g_weapon.c \
                      code/game/ai_chat.c \
                      code/game/ai_cmd.c \
                      code/game/ai_dmnet.c \
                      code/game/ai_dmq3.c \
                      code/game/ai_main.c \
                      code/game/ai_team.c \
                      code/game/ai_vcmd.c \
                      code/game/bg_misc.c \
                      code/game/bg_pmove.c \
                      code/game/bg_slidemove.c \
                      code/game/bg_lib.c

# UI module sources
UI_QVM_SRCS :=        code/q3_ui/ui_main.c \
                      code/q3_ui/ui_addbots.c \
                      code/q3_ui/ui_atoms.c \
                      code/q3_ui/ui_cdkey.c \
                      code/q3_ui/ui_cinematics.c \
                      code/q3_ui/ui_confirm.c \
                      code/q3_ui/ui_connect.c \
                      code/q3_ui/ui_controls2.c \
                      code/q3_ui/ui_credits.c \
                      code/q3_ui/ui_demo2.c \
                      code/q3_ui/ui_display.c \
                      code/q3_ui/ui_gameinfo.c \
                      code/q3_ui/ui_ingame.c \
                      code/q3_ui/ui_loadconfig.c \
                      code/q3_ui/ui_menu.c \
                      code/q3_ui/ui_mfield.c \
                      code/q3_ui/ui_mods.c \
                      code/q3_ui/ui_network.c \
                      code/q3_ui/ui_options.c \
                      code/q3_ui/ui_playermodel.c \
                      code/q3_ui/ui_players.c \
                      code/q3_ui/ui_playersettings.c \
                      code/q3_ui/ui_preferences.c \
                      code/q3_ui/ui_qmenu.c \
                      code/q3_ui/ui_removebots.c \
                      code/q3_ui/ui_saveconfig.c \
                      code/q3_ui/ui_serverinfo.c \
                      code/q3_ui/ui_servers2.c \
                      code/q3_ui/ui_setup.c \
                      code/q3_ui/ui_sound.c \
                      code/q3_ui/ui_sparena.c \
                      code/q3_ui/ui_specifyserver.c \
                      code/q3_ui/ui_splevel.c \
                      code/q3_ui/ui_sppostgame.c \
                      code/q3_ui/ui_spskill.c \
                      code/q3_ui/ui_startserver.c \
                      code/q3_ui/ui_team.c \
                      code/q3_ui/ui_teamorders.c \
                      code/q3_ui/ui_video.c \
                      code/game/bg_misc.c \
                      code/game/bg_lib.c

# ============================================================================
# Auth info for create-fself (homebrew signing)
# ============================================================================

AUTHINFO := "000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"

# ============================================================================
# Build targets
# ============================================================================

# Build QVM files (game module bytecode)
qvms: $(QVM_DIR)/cgame.qvm $(QVM_DIR)/qagame.qvm $(QVM_DIR)/ui.qvm
	@echo "=== QVM files built successfully ==="
	@echo "Output: $(QVM_DIR)/"
	@echo "Copy these to your baseta PK3 as vm/cgame.qvm, vm/qagame.qvm, vm/ui.qvm"

# Package (.pkg)
$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .

# GP4 project file — $(ICON_DST) added so icon copy happens before packaging
pkg.gp4: eboot.bin sce_sys/param.sfo icon
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "eboot.bin sce_sys/param.sfo sce_sys/icon0.png"

# System parameter file
# Match SM64 PS4 param.sfo exactly
# .PHONY-forced: sce_sys/param.sfo is shared across all three Makefiles
# (q3/oa/ta), so a prior variant build can leave a stale param.sfo whose
# CONTENT_ID mismatches the pkg header (SCE_BGFT_ERROR_CONTENTID_UNMATCH at
# install time). Forcing regen every build keeps the sfo in sync.
.PHONY: sce_sys/param.sfo
sce_sys/param.sfo: Makefile
	@mkdir -p sce_sys
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '01.00'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 32832
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE2 --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gde'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ FORMAT --type Utf8 --maxsize 4 --value 'obs'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ PARENTAL_LEVEL --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '01.00'

# ELF -> eboot.bin (signed SELF)
eboot.bin: $(OUTDIR)/ioq3_ps4.elf
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$< -out=$(INTDIR)/ioq3_ps4.oelf --eboot "eboot.bin" --paid 0x3800000000000011 --authinfo $(AUTHINFO)

# Link ELF
$(OUTDIR)/ioq3_ps4.elf: $(ALL_OBJS)
	@mkdir -p $(OUTDIR)
	$(LD) $^ -o $@ $(LDFLAGS) $(LIBS)

# Compile C sources (PS4 engine)
$(INTDIR)/%.o: code/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $<

# Generate stringified GLSL C files from .glsl sources
build/glsl/%.c: code/renderergl2/glsl/%.glsl
	@mkdir -p $(dir $@)
	@echo "const char *fallbackShader_$(notdir $(basename $<)) =" > $@
	@sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $< >> $@
	@echo ";" >> $@

# Compile GLSL stringified shaders
$(INTDIR)/glsl/%.o: build/glsl/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $<

# ============================================================================
# QVM Build Rules
# ============================================================================
# QVMs are built using the host PC toolchain (q3lcc + q3asm).
# They output bytecode that runs on ANY platform via the VM interpreter.
# ============================================================================

# Check for QVM tools
$(Q3LCC):
	@echo "ERROR: q3lcc not found at $(Q3LCC)"
	@echo "You need to build the QVM tools first. From the ioquake3 repo:"
	@echo "  cd code/tools && build_qvm_tools.bat  (on Windows with VS)"
	@echo "Or set Q3LCC to the path of your q3lcc binary"
	@false

$(Q3ASM):
	@echo "ERROR: q3asm not found at $(Q3ASM)"
	@echo "You need to build the QVM tools first. From the ioquake3 repo:"
	@echo "  cd code/tools && build_qvm_tools.bat  (on Windows with VS)"
	@echo "Or set Q3ASM to the path of your q3asm binary"
	@false

# QVM assembly output directory
QVM_ASM_DIR := build/qvm_asm

# CGAME QVM
$(QVM_DIR)/cgame.qvm: $(CGAME_QVM_SRCS) $(Q3LCC) $(Q3ASM)
	@mkdir -p $(QVM_DIR)
	@mkdir -p $(QVM_ASM_DIR)/cgame
	@echo "=== Building cgame.qvm ==="
	@for src in $(CGAME_QVM_SRCS); do \
		base=$$(basename $$src .c); \
		$(Q3LCC) -DCGAME -Icode -Icode/qcommon -o $(QVM_ASM_DIR)/cgame/$$base.asm $$src || exit 1; \
	done
	@cp code/cgame/cg_syscalls.asm $(QVM_ASM_DIR)/cgame/
	$(Q3ASM) -o $@ $(QVM_ASM_DIR)/cgame/*.asm
	@rm -rf $(QVM_ASM_DIR)/cgame

# QAGAME QVM
$(QVM_DIR)/qagame.qvm: $(QAGAME_QVM_SRCS) $(Q3LCC) $(Q3ASM)
	@mkdir -p $(QVM_DIR)
	@mkdir -p $(QVM_ASM_DIR)/qagame
	@echo "=== Building qagame.qvm ==="
	@for src in $(QAGAME_QVM_SRCS); do \
		base=$$(basename $$src .c); \
		$(Q3LCC) -DQAGAME -Icode -Icode/qcommon -Icode/ui -o $(QVM_ASM_DIR)/qagame/$$base.asm $$src || exit 1; \
	done
	@cp code/game/g_syscalls.asm $(QVM_ASM_DIR)/qagame/
	$(Q3ASM) -o $@ $(QVM_ASM_DIR)/qagame/*.asm
	@rm -rf $(QVM_ASM_DIR)/qagame

# UI QVM
$(QVM_DIR)/ui.qvm: $(UI_QVM_SRCS) $(Q3LCC) $(Q3ASM)
	@mkdir -p $(QVM_DIR)
	@mkdir -p $(QVM_ASM_DIR)/ui
	@echo "=== Building ui.qvm ==="
	@for src in $(UI_QVM_SRCS); do \
		base=$$(basename $$src .c); \
		$(Q3LCC) -DUI -Icode -Icode/qcommon -o $(QVM_ASM_DIR)/ui/$$base.asm $$src || exit 1; \
	done
	@cp code/ui/ui_syscalls.asm $(QVM_ASM_DIR)/ui/
	$(Q3ASM) -o $@ $(QVM_ASM_DIR)/ui/*.asm
	@rm -rf $(QVM_ASM_DIR)/ui

# ============================================================================
# Clean
# ============================================================================

clean:
	rm -rf build eboot.bin sce_sys/param.sfo sce_sys/icon0.png pkg.gp4 *.pkg