# ============================================================================
# ioQuake3 PS4 Port -- builds with the OpenOrbis toolchain and Piglet (GLES2).
# ============================================================================

# Package metadata
TITLE       := ioQuake3
VERSION     := 1.00
TITLE_ID    := BREW00003
CONTENT_ID  := IV0000-BREW00003_00-IOQ3PS4PORT00000

# OpenOrbis toolchain root, normalized for MSYS2/bash.
TOOLCHAIN   := $(subst \,/,$(OO_PS4_TOOLCHAIN))

# make            -- release (no runtime log)
# make DEBUG=1    -- debug   (writes /data/ioq3/ioquake3log.txt)
# Separate object dirs make switching modes safe without `make clean`.
DEBUG ?= 0

# Output directories
OUTDIR      := build
ifeq ($(DEBUG),1)
    INTDIR  := build/obj/debug
else
    INTDIR  := build/obj/release
endif

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
# Windows (MSYS2 -> "MSYS_NT-*", Git Bash -> "MINGW64_NT-*", etc.)
ifneq (,$(findstring NT,$(UNAME_S)))
    CDIR    := windows
    # Fall back to the default LLVM install if clang isn't on PATH.
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
# PS4 compilation flags
# ============================================================================

# Sony SCE PS4 triple. The FreeBSD triple links but eglGetDisplay fails on it.
TARGET      := --target=x86_64-scei-ps4-elf

# Disabled features are left undefined (code uses #ifdef, not #if).
DEFINES     := -D__ORBIS__ -D__PS4__ \
               -DBOTLIB \
               -DUSE_INTERNAL_JPEG=1 \
               -DUSE_INTERNAL_ZLIB=1 \
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
               -c

CFLAGS      += -Wno-unused-variable -Wno-unused-function \
               -Wno-incompatible-pointer-types -Wno-int-conversion

# crt1.o is placed before -L/-l: link order is objects, crt1, then libs.
LDFLAGS     := -m elf_x86_64 -pie \
               --script $(TOOLCHAIN)/link.x \
               --eh-frame-hdr \
               ps4_crt1.o \
               -L$(TOOLCHAIN)/lib

# ============================================================================
# Libraries
# ============================================================================

# Link against Sony's SceLibcInternal, NOT the OpenOrbis musl libc.prx:
# Piglet depends on SceLibcInternal, and using musl makes eglGetDisplay
# fail silently. libc.prx must therefore NOT be present in sce_module/.
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
PS4_SRCS := \
    code/ps4/sys_main_ps4.c \
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
RENDERER_SRCS := \
    code/renderergl2/tr_animation.c \
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
RENDERERCOMMON_SRCS := \
    code/renderercommon/puff.c \
    code/renderercommon/tr_font.c \
    code/renderercommon/tr_image_bmp.c \
    code/renderercommon/tr_image_jpg.c \
    code/renderercommon/tr_image_pcx.c \
    code/renderercommon/tr_image_png.c \
    code/renderercommon/tr_image_pvr.c \
    code/renderercommon/tr_image_tga.c \
    code/renderercommon/tr_noise.c

# Client
CLIENT_SRCS := \
    code/client/cl_avi.c \
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
    code/client/snd_dma.c \
    code/client/snd_main.c \
    code/client/snd_mem.c \
    code/client/snd_mix.c \
    code/client/snd_wavelet.c

# Common engine
QCOMMON_SRCS := \
    code/qcommon/cm_load.c \
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
SERVER_SRCS := \
    code/server/sv_bot.c \
    code/server/sv_ccmds.c \
    code/server/sv_client.c \
    code/server/sv_game.c \
    code/server/sv_init.c \
    code/server/sv_main.c \
    code/server/sv_net_chan.c \
    code/server/sv_snapshot.c \
    code/server/sv_world.c

# Bot library
BOTLIB_SRCS := \
    code/botlib/be_aas_bspq3.c \
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

# Third-party (bundled)
THIRDPARTY_SRCS := \
    code/thirdparty/zlib-1.3.1/adler32.c \
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
    code/thirdparty/jpeg-9f/jutils.c

# Architecture-specific
ASM_SRCS := \
    code/asm/snapvector.c \
    code/asm/ftola.c

# GLSL stringified shaders (pre-generated in build/glsl/)
GLSL_NAMES := \
    bokeh_fp bokeh_vp \
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

# All sources
ALL_SRCS := $(PS4_SRCS) $(RENDERER_SRCS) $(RENDERERCOMMON_SRCS) \
            $(CLIENT_SRCS) $(QCOMMON_SRCS) $(SERVER_SRCS) \
            $(BOTLIB_SRCS) $(THIRDPARTY_SRCS) $(ASM_SRCS)

# Object files
ALL_OBJS := $(patsubst code/%.c, $(INTDIR)/%.o, $(ALL_SRCS)) $(GLSL_OBJS)

# ============================================================================
# Auth info for create-fself (homebrew signing)
# ============================================================================

AUTHINFO := "000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"

# ============================================================================
# Build targets
# ============================================================================

.PHONY: all release debug clean

all: release

release: $(CONTENT_ID).pkg

debug:
	$(MAKE) DEBUG=1 $(CONTENT_ID).pkg

# Package (.pkg)
$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .

# GP4 project file. sce_module/ is intentionally empty: no libc.prx
# (we use SceLibcInternal) and no libSceFios2.prx.
ifeq ($(DEBUG),1)
    PKG_ICON := sce_sys/icon1.png
else
    PKG_ICON := sce_sys/icon0.png
endif

pkg.gp4: eboot.bin sce_sys/param.sfo $(PKG_ICON)
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "eboot.bin sce_sys/param.sfo $(PKG_ICON)"

# System parameter file.
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

# Compile C sources
$(INTDIR)/%.o: code/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $<

# Generate stringified GLSL C files from .glsl sources
build/glsl/%.c: code/renderergl2/glsl/%.glsl
	@mkdir -p $(dir $@)
	@echo "const char *fallbackShader_$(notdir $(basename $<)) =" > $@
	@sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/\"/' -e 's/$$/\\n\"/' $< >> $@
	@echo ";" >> $@

# Compile GLSL stringified shaders (from build/glsl/)
$(INTDIR)/glsl/%.o: build/glsl/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $<

# ============================================================================
# Clean
# ============================================================================

clean:
	rm -rf build eboot.bin sce_sys/param.sfo pkg.gp4 *.pkg
