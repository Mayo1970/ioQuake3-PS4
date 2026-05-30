# ============================================================================
# ioQuake3 PS4 Port - Unified Makefile
#
# Supports:
#   make            -- Standard Quake 3 build
#   make ta         -- Team Arena build
#   make oa         -- OpenArena build
#   make all-flavors-- Build all three (Q3, TA, OA) release packages
#   make release    -- Explicit release build
#   make debug      -- Debug build
# ============================================================================

# ============================================================================
# Flavor selection (default: q3, set TA=1 or OA=1, or run `make ta` / `make oa`)
# ============================================================================

# Allow `make ta` and `make oa` as shorthands
ifeq ($(MAKECMDGOALS),ta)
    TA := 1
endif
ifeq ($(MAKECMDGOALS),oa)
    OA := 1
endif

# Determine flavor (q3 is default)
ifeq ($(TA),1)
    FLAVOR      := ta
    TITLE       := Quake 3: Team Arena
    TITLE_ID    := QUAK03001
    CONTENT_ID  := IV0000-QUAK03001_00-IOQ3PS4PORT00000
    ICON_SRC    := icons/ta/icon0.png
    DEFINES_EXTRA := -DSTANDALONETA
else ifeq ($(OA),1)
    FLAVOR      := oa
    TITLE       := Open Arena
    TITLE_ID    := QUAK03002
    CONTENT_ID  := IV0000-QUAK03002_00-IOQ3PS4PORT00000
    ICON_SRC    := icons/oa/icon0.png
    DEFINES_EXTRA := -DSTANDALONEOA
else
    FLAVOR      := q3
    TITLE       := ioQuake 3
    TITLE_ID    := QUAK03000
    CONTENT_ID  := IV0000-QUAK03000_00-IOQ3PS4PORT00000
    ICON_SRC    := icons/q3/icon0.png
    DEFINES_EXTRA :=
endif

VERSION     := 1.01

# ============================================================================
# Ensure sce_sys directory exists
# ============================================================================

$(shell mkdir -p sce_sys)

# ============================================================================
# Default target MUST be first
# ============================================================================

.PHONY: all release debug clean icon ta oa all-flavors

all: release

ta: release
	@true

oa: release
	@true

release: $(CONTENT_ID).pkg

debug:
	$(MAKE) DEBUG=1 $(CONTENT_ID).pkg

# ============================================================================
# Multi-flavor build target
# ============================================================================

all-flavors:
	@echo "=== Building Quake 3 ==="
	$(MAKE) release
	@echo "=== Building Team Arena ==="
	$(MAKE) ta
	@echo "=== Building OpenArena ==="
	$(MAKE) oa
	@echo "=== All builds complete ==="
	@echo "Outputs:"
	@ls -1 IV0000-QUAK03000_00-IOQ3PS4PORT00000.pkg 2>/dev/null || true
	@ls -1 IV0000-QUAK03001_00-IOQ3PS4PORT00000.pkg 2>/dev/null || true
	@ls -1 IV0000-QUAK03002_00-IOQ3PS4PORT00000.pkg 2>/dev/null || true


# ============================================================================
# OpenOrbis toolchain
# ============================================================================

TOOLCHAIN   := $(subst \,/,$(OO_PS4_TOOLCHAIN))

DEBUG ?= 0

OUTDIR      := build
ifeq ($(DEBUG),1)
    INTDIR  := build/obj/$(FLAVOR)/debug
else
    INTDIR  := build/obj/$(FLAVOR)/release
endif

# ============================================================================
# Icon
# ============================================================================

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
ifneq (,$(findstring NT,$(UNAME_S)))
    CDIR    := windows
    ifeq ($(shell which clang 2>/dev/null),)
        _LLVM_TRY := $(shell ls -d "/c/Program Files/LLVM/bin" 2>/dev/null || \
                               ls -d "/c/LLVM/bin" 2>/dev/null || echo "")
        ifneq ($(_LLVM_TRY),)
            export PATH := $(_LLVM_TRY):$(PATH)
        endif
    endif
    CC      := clang
    CXX     := clang++
    LD      := ld.lld
endif
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
# PS4 compilation flags
# ============================================================================

TARGET      := --target=x86_64-scei-ps4-elf

DEFINES     := -D__ORBIS__ -D__PS4__ \
               -DBOTLIB \
               -DUSE_INTERNAL_JPEG=1 \
               -DUSE_INTERNAL_ZLIB=1 \
               -DUSE_CODEC_VORBIS=1 \
               $(DEFINES_EXTRA) \
               -DPRODUCT_VERSION=\"1.36_ps4\"

ifeq ($(DEBUG),1)
    DEFINES += -DPS4_DEBUG
endif

ifeq ($(DEBUG),1)
    OPTFLAGS := -O0 -g
else
    OPTFLAGS := -O2
endif

CFLAGS      := $(TARGET) -fPIC -funwind-tables -fexceptions \
               $(OPTFLAGS) \
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

CFLAGS      += -Wno-unused-variable -Wno-unused-function \
               -Wno-incompatible-pointer-types -Wno-int-conversion

LDFLAGS     := -m elf_x86_64 -pie \
               --script $(TOOLCHAIN)/link.x \
               --eh-frame-hdr \
               ps4_crt1.o \
               -L$(TOOLCHAIN)/lib

# ============================================================================
# Libraries
# ============================================================================

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
               -lSceImeDialog \

# ============================================================================
# Source files (shared between all flavors)
# ============================================================================

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

RENDERERCOMMON_SRCS :=     code/renderercommon/puff.c \
                           code/renderercommon/tr_font.c \
                           code/renderercommon/tr_image_bmp.c \
                           code/renderercommon/tr_image_jpg.c \
                           code/renderercommon/tr_image_pcx.c \
                           code/renderercommon/tr_image_png.c \
                           code/renderercommon/tr_image_pvr.c \
                           code/renderercommon/tr_image_tga.c \
                           code/renderercommon/tr_noise.c

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

SERVER_SRCS :=     code/server/sv_bot.c \
                   code/server/sv_ccmds.c \
                   code/server/sv_client.c \
                   code/server/sv_game.c \
                   code/server/sv_init.c \
                   code/server/sv_main.c \
                   code/server/sv_net_chan.c \
                   code/server/sv_snapshot.c \
                   code/server/sv_world.c

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

ASM_SRCS :=     code/asm/snapvector.c \
                code/asm/ftola.c

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

ALL_SRCS := $(PS4_SRCS) $(RENDERER_SRCS) $(RENDERERCOMMON_SRCS) \
            $(CLIENT_SRCS) $(QCOMMON_SRCS) $(SERVER_SRCS) \
            $(BOTLIB_SRCS) $(THIRDPARTY_SRCS) $(ASM_SRCS)

ALL_OBJS := $(patsubst code/%.c, $(INTDIR)/%.o, $(ALL_SRCS)) $(GLSL_OBJS)

# ============================================================================
# Auth info
# ============================================================================

AUTHINFO := "000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"

# ============================================================================
# Flavor-specific output filenames
# ============================================================================

ELF_FILE     := $(OUTDIR)/ioq3_ps4_$(FLAVOR).elf
OELF_FILE    := $(INTDIR)/ioq3_ps4_$(FLAVOR).oelf

# ============================================================================
# Flavor stamp — forces rebuild of shared files when flavor changes
# ============================================================================

FLAVOR_STAMP := build/.flavor_$(FLAVOR)

$(FLAVOR_STAMP):
	@mkdir -p build
	@rm -f build/.flavor_*
	@echo "$(FLAVOR)" > $@

# ============================================================================
# Build targets
# ============================================================================

$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .

# All files/dirs in fixes/, then filtered per flavor below
FIX_FILES_ALL := $(shell find fixes -type f 2>/dev/null)
FIX_DIRS      := $(shell find fixes -type d 2>/dev/null)

# Each flavor only bundles its own splash zip and relevant pk3 dirs.
# Exclude splash zips for the other two flavors.
ifeq ($(FLAVOR),ta)
    FIX_FILES := $(filter-out fixes/splash.zip fixes/oa.zip, $(FIX_FILES_ALL))
else ifeq ($(FLAVOR),oa)
    FIX_FILES := $(filter-out fixes/splash.zip fixes/ta.zip, $(FIX_FILES_ALL))
else
    FIX_FILES := $(filter-out fixes/ta.zip fixes/oa.zip, $(FIX_FILES_ALL))
endif

# Emit one <file> entry per fix file
define GP4_FILE_ENTRY
		<file targ_path="$(1)" orig_path="$(1)" />
endef

# Emit one <dir> entry per fix directory (skip the root "fixes" entry itself,
# PkgTool only needs the named children; the root dir tag wraps them).
define GP4_DIR_ENTRY
		<dir targ_name="$(notdir $(1))" />
endef

pkg.gp4: eboot.bin sce_sys/param.sfo icon $(FLAVOR_STAMP)
	@printf '<?xml version="1.0"?>\n' > $@
	@printf '<psproject fmt="gp4" version="1000">\n' >> $@
	@printf '\t<volume>\n' >> $@
	@printf '\t\t<volume_type>pkg_ps4_app</volume_type>\n' >> $@
	@printf '\t\t<volume_id>PS4VOLUME</volume_id>\n' >> $@
	@printf '\t\t<volume_ts>2026-01-01 00:00:00</volume_ts>\n' >> $@
	@printf '\t\t<package content_id="$(CONTENT_ID)" passcode="00000000000000000000000000000000" storage_type="digital50" app_type="full" />\n' >> $@
	@printf '\t\t<chunk_info chunk_count="1" scenario_count="1">\n' >> $@
	@printf '\t\t\t<chunks><chunk id="0" layer_no="0" label="Chunk #0" /></chunks>\n' >> $@
	@printf '\t\t\t<scenarios default_id="0"><scenario id="0" type="sp" initial_chunk_count="1" label="Scenario #0">0</scenario></scenarios>\n' >> $@
	@printf '\t\t</chunk_info>\n' >> $@
	@printf '\t</volume>\n' >> $@
	@printf '\t<files img_no="0">\n' >> $@
	@printf '\t\t<file targ_path="eboot.bin" orig_path="eboot.bin" />\n' >> $@
	@printf '\t\t<file targ_path="sce_sys/param.sfo" orig_path="sce_sys/param.sfo" />\n' >> $@
	@printf '\t\t<file targ_path="sce_sys/icon0.png" orig_path="sce_sys/icon0.png" />\n' >> $@
	@$(foreach f,$(FIX_FILES),printf '\t\t<file targ_path="$(f)" orig_path="$(f)" />\n' >> $@;)
	@printf '\t</files>\n' >> $@
	@printf '\t<rootdir>\n' >> $@
	@printf '\t\t<dir targ_name="sce_sys"><dir targ_name="about" /></dir>\n' >> $@
	@printf '\t\t<dir targ_name="sce_module" />\n' >> $@
	@printf '\t\t<dir targ_name="fixes">\n' >> $@
	@printf '\t\t\t<dir targ_name="baseq3">\n' >> $@
	@$(foreach d,$(filter fixes/baseq3/%,$(FIX_DIRS)),printf '\t\t\t\t<dir targ_name="$(notdir $(d))" />\n' >> $@;)
	@printf '\t\t\t</dir>\n' >> $@
	@printf '\t\t\t<dir targ_name="missionpack">\n' >> $@
	@$(foreach d,$(filter fixes/missionpack/%,$(FIX_DIRS)),printf '\t\t\t\t<dir targ_name="$(notdir $(d))" />\n' >> $@;)
	@printf '\t\t\t</dir>\n' >> $@
	@printf '\t\t\t<dir targ_name="baseoa">\n' >> $@
	@$(foreach d,$(filter fixes/baseoa/%,$(FIX_DIRS)),printf '\t\t\t\t<dir targ_name="$(notdir $(d))" />\n' >> $@;)
	@printf '\t\t\t</dir>\n' >> $@
	@printf '\t\t</dir>\n' >> $@
	@printf '\t</rootdir>\n' >> $@
	@printf '</psproject>\n' >> $@

sce_sys/param.sfo: Makefile $(FLAVOR_STAMP)
	@mkdir -p sce_sys
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '01.00'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0x00000042
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE2 --type Integer --maxsize 4 --value 0x00000002
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gde'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ FORMAT --type Utf8 --maxsize 4 --value 'obs'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ PARENTAL_LEVEL --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ REMOTE_PLAY_KEY_ASSIGN --type Integer --maxsize 4 --value 1
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SERVICE_ID_ADDCONT_ADD_1 --type Utf8 --maxsize 20 --value ''
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SERVICE_ID_ADDCONT_ADD_2 --type Utf8 --maxsize 20 --value ''
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SERVICE_ID_ADDCONT_ADD_3 --type Utf8 --maxsize 20 --value ''
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SERVICE_ID_ADDCONT_ADD_4 --type Utf8 --maxsize 20 --value ''
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SERVICE_ID_ADDCONT_ADD_5 --type Utf8 --maxsize 20 --value ''
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SERVICE_ID_ADDCONT_ADD_6 --type Utf8 --maxsize 20 --value ''
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SERVICE_ID_ADDCONT_ADD_7 --type Utf8 --maxsize 20 --value ''
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '01.00'

eboot.bin: $(ELF_FILE) $(FLAVOR_STAMP)
	@mkdir -p $(INTDIR)
ifeq ($(DEBUG),1)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$< -out=$(OELF_FILE) --eboot "eboot.bin" --paid 0x3800000000000011 --authinfo $(AUTHINFO)
else
	llvm-strip --strip-debug $< -o $<.stripped
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$<.stripped -out=$(OELF_FILE) --eboot "eboot.bin" --paid 0x3800000000000011 --authinfo $(AUTHINFO)
endif

$(ELF_FILE): $(ALL_OBJS)
	@mkdir -p $(OUTDIR)
	$(LD) $^ -o $@ $(LDFLAGS) $(LIBS)

$(INTDIR)/%.o: code/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $<

build/glsl/%.c: code/renderergl2/glsl/%.glsl
	@mkdir -p $(dir $@)
	@echo "const char *fallbackShader_$(notdir $(basename $<)) =" > $@
	@sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $< >> $@
	@echo ";" >> $@

$(INTDIR)/glsl/%.o: build/glsl/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $<

# ============================================================================
# Clean
# ============================================================================

clean:
	rm -rf build eboot.bin sce_sys/param.sfo sce_sys/icon0.png pkg.gp4 *.pkg sce_sys *.stripped