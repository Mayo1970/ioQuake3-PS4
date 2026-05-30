/* ps4_glimp.c -- PS4 Piglet/EGL GLES2 context management. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <orbis/Pigletv2VSH.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>

#include "../renderercommon/tr_common.h"
#include "../sys/sys_local.h"
#include "../qcommon/unzip.h"

/* GLES version globals (declared extern in qgl.h). */
int qglMajorVersion = 0;
int qglMinorVersion = 0;
int qglesMajorVersion = 2;
int qglesMinorVersion = 0;

static EGLDisplay s_display = EGL_NO_DISPLAY;
static EGLSurface s_surface = EGL_NO_SURFACE;
static EGLContext  s_context = EGL_NO_CONTEXT;

static int s_pigletModuleId  = -1;
static int s_shaccModuleId   = -1;
static qboolean s_pigletConfigured = qfalse;

/* Splash screen, drawn once right after the GL context is up. Source image:
   magick logo.bmp -resize "1920x1080^" -gravity center -extent 1920x1080 -flip -depth 8 RGBA:splash.rgba */
#ifdef STANDALONETA
#define SPLASH_PATH    "/app0/fixes/ta.zip"
#elif defined(STANDALONEOA)
#define SPLASH_PATH    "/app0/fixes/oa.zip"
#else
#define SPLASH_PATH    "/app0/fixes/splash.zip"
#endif
#define SPLASH_WIDTH   1920
#define SPLASH_HEIGHT  1080
#define SPLASH_SIZE    (SPLASH_WIDTH * SPLASH_HEIGHT * 4)

static const char *splash_vert =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "    v_uv = a_uv;\n"
    "}\n";

static const char *splash_frag =
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_tex, v_uv);\n"
    "}\n";

static GLuint splash_prog;
static GLuint splash_vbo;
static GLuint splash_tex;

static GLuint Splash_CompileShader(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    GLint compiled;

    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        Com_Printf("Splash: shader compile failed: %s\n", log);
        glDeleteShader(s);
        return 0;
    }

    return s;
}

static void Splash_CreateResources(void)
{
    GLuint vs, fs;
    GLint linked;

    vs = Splash_CompileShader(GL_VERTEX_SHADER, splash_vert);
    fs = Splash_CompileShader(GL_FRAGMENT_SHADER, splash_frag);

    if (!vs || !fs) {
        Com_Printf("Splash: shader compilation failed, skipping splash\n");
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return;
    }

    splash_prog = glCreateProgram();
    glAttachShader(splash_prog, vs);
    glAttachShader(splash_prog, fs);
    glBindAttribLocation(splash_prog, 0, "a_pos");
    glBindAttribLocation(splash_prog, 1, "a_uv");
    glLinkProgram(splash_prog);
    glGetProgramiv(splash_prog, GL_LINK_STATUS, &linked);

    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!linked) {
        char log[512];
        glGetProgramInfoLog(splash_prog, sizeof(log), NULL, log);
        Com_Printf("Splash: program link failed: %s\n", log);
        glDeleteProgram(splash_prog);
        splash_prog = 0;
        return;
    }

    /* Fullscreen quad: 2 triangles, pos + uv interleaved */
    float verts[] = {
        /* pos        uv     */
        -1.0f, -1.0f, 0.0f, 0.0f,   /* bottom-left */
         1.0f, -1.0f, 1.0f, 0.0f,   /* bottom-right */
        -1.0f,  1.0f, 0.0f, 1.0f,   /* top-left */

        -1.0f,  1.0f, 0.0f, 1.0f,   /* top-left */
         1.0f, -1.0f, 1.0f, 0.0f,   /* bottom-right */
         1.0f,  1.0f, 1.0f, 1.0f,   /* top-right */
    };

    glGenBuffers(1, &splash_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, splash_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenTextures(1, &splash_tex);
    glBindTexture(GL_TEXTURE_2D, splash_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void Splash_DestroyResources(void)
{
    if (splash_prog) {
        glDeleteProgram(splash_prog);
        splash_prog = 0;
    }
    if (splash_vbo) {
        glDeleteBuffers(1, &splash_vbo);
        splash_vbo = 0;
    }
    if (splash_tex) {
        glDeleteTextures(1, &splash_tex);
        splash_tex = 0;
    }
}

static qboolean Splash_LoadTexture(void)
{
    unzFile uf;
    unz_file_info file_info;
    unsigned char *pixels = NULL;
    int ret;
    qboolean result = qfalse;

    uf = unzOpen(SPLASH_PATH);
    if (!uf) {
        Com_Printf("Splash: unzOpen failed for %s\n", SPLASH_PATH);
        return qfalse;
    }

    ret = unzLocateFile(uf, "splash.rgba", 2); /* 2 = case-insensitive */
    if (ret != UNZ_OK) {
        Com_Printf("Splash: unzLocateFile failed (%d)\n", ret);
        unzClose(uf);
        return qfalse;
    }

    ret = unzGetCurrentFileInfo(uf, &file_info, NULL, 0, NULL, 0, NULL, 0);
    if (ret != UNZ_OK || file_info.uncompressed_size != SPLASH_SIZE) {
        Com_Printf("Splash: bad file size (%lu != %d)\n",
                   (unsigned long)file_info.uncompressed_size, SPLASH_SIZE);
        unzClose(uf);
        return qfalse;
    }

    ret = unzOpenCurrentFile(uf);
    if (ret != UNZ_OK) {
        Com_Printf("Splash: unzOpenCurrentFile failed (%d)\n", ret);
        unzClose(uf);
        return qfalse;
    }

    pixels = (unsigned char *)malloc(SPLASH_SIZE);
    if (!pixels) {
        Com_Printf("Splash: failed to allocate pixel buffer\n");
        unzCloseCurrentFile(uf);
        unzClose(uf);
        return qfalse;
    }

    ret = unzReadCurrentFile(uf, pixels, SPLASH_SIZE);
    if (ret != SPLASH_SIZE) {
        Com_Printf("Splash: unzReadCurrentFile failed (%d)\n", ret);
        free(pixels);
        unzCloseCurrentFile(uf);
        unzClose(uf);
        return qfalse;
    }

    unzCloseCurrentFile(uf);
    unzClose(uf);

    glBindTexture(GL_TEXTURE_2D, splash_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SPLASH_WIDTH, SPLASH_HEIGHT,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    result = qtrue;

    free(pixels);
    return result;
}

static void Splash_DrawOnce(void)
{
    GLint prevProg;
    GLint prevTex;
    GLint prevArrayBuf;
    GLboolean blend, depthTest, cullFace;
    GLint viewport[4];

    /* Save renderer state so we don't interfere */
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevArrayBuf);
    blend = glIsEnabled(GL_BLEND);
    depthTest = glIsEnabled(GL_DEPTH_TEST);
    cullFace = glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(GL_VIEWPORT, viewport);

    if (!splash_prog)
        Splash_CreateResources();

    if (!splash_prog || !Splash_LoadTexture()) {
        glUseProgram(prevProg);
        glBindTexture(GL_TEXTURE_2D, prevTex);
        glBindBuffer(GL_ARRAY_BUFFER, prevArrayBuf);
        if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (cullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        return;
    }

    glViewport(0, 0, glConfig.vidWidth, glConfig.vidHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glUseProgram(splash_prog);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, splash_tex);
    glUniform1i(glGetUniformLocation(splash_prog, "u_tex"), 0);

    glBindBuffer(GL_ARRAY_BUFFER, splash_vbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void *)(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void *)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    eglSwapBuffers(s_display, s_surface);

    Splash_DestroyResources();

    glUseProgram(prevProg);
    glBindTexture(GL_TEXTURE_2D, prevTex);
    glBindBuffer(GL_ARRAY_BUFFER, prevArrayBuf);
    if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

/* Try /data/self/.../lib, sandbox path, then /app0/sce_module/. */
static qboolean PS4_LoadPigletModules(void)
{
	s_pigletModuleId = sceKernelLoadStartModule(
		"/data/self/system/common/lib/libScePigletv2VSH.sprx",
		0, NULL, 0, NULL, NULL);
	Com_Printf("  Piglet (/data/): 0x%08X\n", s_pigletModuleId);

	if (s_pigletModuleId < 0) {
		const char *sw = sceKernelGetFsSandboxRandomWord();
		char path[256];
		snprintf(path, sizeof(path), "/%s/common/lib/libScePigletv2VSH.sprx", sw);
		s_pigletModuleId = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, NULL);
		Com_Printf("  Piglet (sandbox): 0x%08X\n", s_pigletModuleId);
	}

	if (s_pigletModuleId < 0) {
		s_pigletModuleId = sceKernelLoadStartModule(
			"/app0/sce_module/libScePigletv2VSH.sprx",
			0, NULL, 0, NULL, NULL);
		Com_Printf("  Piglet (app0): 0x%08X\n", s_pigletModuleId);
	}

	if (s_pigletModuleId < 0) {
		Com_Printf("ERROR: Failed to load Piglet module: 0x%08X\n", s_pigletModuleId);
		return qfalse;
	}

	s_shaccModuleId = sceKernelLoadStartModule(
		"/data/self/system/common/lib/libSceShaccVSH.sprx",
		0, NULL, 0, NULL, NULL);
	Com_Printf("  ShaccVSH (/data/): 0x%08X\n", s_shaccModuleId);

	if (s_shaccModuleId < 0) {
		const char *sw = sceKernelGetFsSandboxRandomWord();
		char path[256];
		snprintf(path, sizeof(path), "/%s/common/lib/libSceShaccVSH.sprx", sw);
		s_shaccModuleId = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, NULL);
		Com_Printf("  ShaccVSH (sandbox): 0x%08X\n", s_shaccModuleId);
	}

	if (s_shaccModuleId >= 0) {
		Com_Printf("Shader compiler (ShaccVSH) loaded, runtime GLSL available\n");
	} else {
		Com_Printf("WARNING: Shader compiler not found: 0x%08X\n", s_shaccModuleId);
		Com_Printf("  glShaderSource/glCompileShader will not work.\n");
	}

	return qtrue;
}

void GLimp_Init(qboolean fixedFunction)
{
	EGLint major, minor;
	EGLConfig egl_config;
	EGLint num_configs;
	int width, height;

	Com_Printf("Initializing PS4 OpenGL ES 2.0 (Piglet)...\n");

	/* Piglet rejects non-standard resolutions at eglSwapBuffers; force 1080p. */
	width = 1920;
	height = 1080;
	ri.Cvar_Set("r_customwidth", "1920");
	ri.Cvar_Set("r_customheight", "1080");
	//ri.Cvar_Set("r_mode", "-1"); //don't do this or it breaks us from changing graphics settings like textures

	/* Shadow FBOs return GL_FRAMEBUFFER_UNSUPPORTED on Piglet. */
	ri.Cvar_Set("cg_shadows", "0");
	ri.Cvar_Set("r_sunShadows", "0");

#ifdef STANDALONEOA
	/* OA default ui_browserMaster=0 wedges Favorites on a never-completing query. */
	ri.Cvar_Set("ui_browserMaster", "1");
#endif

	/* One-time per process; re-calling on vid_restart/mod switch crashes. */
	if (!s_pigletConfigured) {
		if (!PS4_LoadPigletModules()) {
			Com_Error(ERR_FATAL, "GLimp_Init: Failed to load Piglet modules");
			return;
		}

		{
			OrbisPglConfig pgl_config;
			memset(&pgl_config, 0, sizeof(pgl_config));
			pgl_config.size = sizeof(OrbisPglConfig);
			pgl_config.flags = ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT
			                 | ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY
			                 | 0x60;
			pgl_config.processOrder = 1;
			pgl_config.systemSharedMemorySize = 250ULL*1024*1024;
			pgl_config.videoSharedMemorySize  = 512ULL*1024*1024;
			pgl_config.maxMappedFlexibleMemory = 170ULL*1024*1024;
			pgl_config.drawCommandBufferSize = 1*1024*1024;
			pgl_config.lcueResourceBufferSize = 1*1024*1024;
			pgl_config.dbgPosCmd_0x40 = width;
			pgl_config.dbgPosCmd_0x44 = height;
			pgl_config.dbgPosCmd_0x48 = 0;
			pgl_config.dbgPosCmd_0x4C = 0;
			pgl_config.unk_0x5C = 2;

			if (!scePigletSetConfigurationVSH(&pgl_config)) {
				Com_Printf("WARNING: scePigletSetConfigurationVSH failed\n");
			} else {
				Com_Printf("scePigletSetConfigurationVSH succeeded\n");
			}
		}

		s_pigletConfigured = qtrue;
	} else {
		Com_Printf("PS4 GL: reusing existing Piglet instance (mod/vid restart)\n");
	}

	/* Display kept alive across restarts: eglTerminate breaks Piglet re-init. */
	if (s_display == EGL_NO_DISPLAY) {
		Com_Printf("Calling eglGetDisplay(EGL_DEFAULT_DISPLAY)...\n");
		s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		Com_Printf("eglGetDisplay returned %p (err=0x%X)\n",
			(void *)s_display, eglGetError());

		if (s_display == EGL_NO_DISPLAY) {
			Com_Error(ERR_FATAL, "GLimp_Init: eglGetDisplay failed (0x%X)", eglGetError());
			return;
		}

		if (!eglInitialize(s_display, &major, &minor)) {
			Com_Error(ERR_FATAL, "GLimp_Init: eglInitialize failed");
			return;
		}
		Com_Printf("EGL %d.%d initialized\n", major, minor);
	} else {
		Com_Printf("PS4 GL: reusing EGL display (mod/vid restart)\n");
	}

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		Com_Error(ERR_FATAL, "GLimp_Init: eglBindAPI failed");
		return;
	}

	/* Depth/stencil 0: renderergl2 uses FBOs; no Piglet config matches 24/8. */
	EGLint config_attribs[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 0,
		EGL_STENCIL_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE
	};

	if (!eglChooseConfig(s_display, config_attribs, &egl_config, 1, &num_configs)
		|| num_configs < 1) {
		Com_Error(ERR_FATAL, "GLimp_Init: eglChooseConfig failed");
		return;
	}

	/* Piglet's EGLNativeWindowType is exactly this 16-byte layout. */
	static struct { uint32_t id; uint32_t w; uint32_t h; uint32_t pad; } s_render_window;
	s_render_window.id = 0;
	s_render_window.w = width;
	s_render_window.h = height;
	s_render_window.pad = 0;

	EGLint window_attribs[] = {
		EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
		EGL_NONE
	};

	s_surface = eglCreateWindowSurface(s_display, egl_config,
		(EGLNativeWindowType)&s_render_window, window_attribs);
	if (s_surface == EGL_NO_SURFACE) {
		Com_Error(ERR_FATAL, "GLimp_Init: eglCreateWindowSurface failed (0x%X)",
			eglGetError());
		return;
	}

	EGLint ctx_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	s_context = eglCreateContext(s_display, egl_config, EGL_NO_CONTEXT, ctx_attribs);
	if (s_context == EGL_NO_CONTEXT) {
		Com_Error(ERR_FATAL, "GLimp_Init: eglCreateContext failed (0x%X)",
			eglGetError());
		return;
	}

	if (!eglMakeCurrent(s_display, s_surface, s_surface, s_context)) {
		Com_Error(ERR_FATAL, "GLimp_Init: eglMakeCurrent failed (0x%X)",
			eglGetError());
		return;
	}

	eglSwapInterval(s_display, 0);

	glConfig.vidWidth = width;
	glConfig.vidHeight = height;
	glConfig.colorBits = 32;
	glConfig.depthBits = 0;
	glConfig.stencilBits = 0;
	glConfig.isFullscreen = qtrue;
	glConfig.windowAspect = (float)width / (float)height;
	glConfig.stereoEnabled = qfalse;
	glConfig.smpActive = qfalse;
	glConfig.textureCompression = TC_NONE;

	// Force 16 bit textures
	ri.Cvar_Set("r_texturebits", "16");
	glConfig.deviceSupportsGamma = qfalse;

	Q_strncpyz(glConfig.vendor_string,
		(const char *)qglGetString(GL_VENDOR), sizeof(glConfig.vendor_string));
	Q_strncpyz(glConfig.renderer_string,
		(const char *)qglGetString(GL_RENDERER), sizeof(glConfig.renderer_string));
	Q_strncpyz(glConfig.version_string,
		(const char *)qglGetString(GL_VERSION), sizeof(glConfig.version_string));
	Q_strncpyz(glConfig.extensions_string,
		(const char *)qglGetString(GL_EXTENSIONS), sizeof(glConfig.extensions_string));

	Com_Printf("GL_VENDOR: %s\n", glConfig.vendor_string);
	Com_Printf("GL_RENDERER: %s\n", glConfig.renderer_string);
	Com_Printf("GL_VERSION: %s\n", glConfig.version_string);

	Com_Printf("PS4 GLES2 context ready (%dx%d)\n", width, height);

	Splash_DrawOnce();

	ri.IN_Init(NULL);
}

void GLimp_EndFrame(void)
{
	if (s_display != EGL_NO_DISPLAY && s_surface != EGL_NO_SURFACE) {
		eglSwapBuffers(s_display, s_surface);
	}
}

void GLimp_Shutdown(void)
{
	ri.IN_Shutdown();

	if (s_display != EGL_NO_DISPLAY) {
		eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

		if (s_context != EGL_NO_CONTEXT) {
			eglDestroyContext(s_display, s_context);
			s_context = EGL_NO_CONTEXT;
		}

		if (s_surface != EGL_NO_SURFACE) {
			eglDestroySurface(s_display, s_surface);
			s_surface = EGL_NO_SURFACE;
		}

		/* No eglTerminate: Piglet cannot be re-initialized in a process. */
	}

	Com_Printf("PS4 GL context shut down\n");
}

void GLimp_Minimize(void)
{
}

void GLimp_LogComment(char *comment)
{
}
