/* ps4_glimp.c -- PS4 Piglet/EGL GLES2 context management. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <orbis/Pigletv2VSH.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>

#include "../renderercommon/tr_common.h"
#include "../sys/sys_local.h"

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

/* Load Piglet (GLES2) and ShaccVSH (runtime GLSL compiler) sprx modules.
 * Tries /data/self/system/common/lib/ first (RetroArch PS4 install location),
 * then the sandbox random-word path, then /app0/sce_module/ as a last resort. */
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

/* Initialize Piglet, EGL, and create the GLES2 rendering context. */
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
	ri.Cvar_Set("r_mode", "-1");

	/* Shadow FBOs return GL_FRAMEBUFFER_UNSUPPORTED on Piglet. */
	ri.Cvar_Set("cg_shadows", "0");
	ri.Cvar_Set("r_sunShadows", "0");

	/* Module loading and scePigletSetConfigurationVSH are one-time per process;
	 * calling them again on vid_restart/mod switch crashes. */
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

	/* EGL display is kept alive across vid_restart/mod switch (eglTerminate
	 * cannot be followed by a working eglInitialize on Piglet). */
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

	/* Depth/stencil 0: renderergl2 uses FBOs for its own depth/stencil.
	 * Requesting 24/8 has no matching Piglet EGL config. */
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

	/* Piglet's EGLNativeWindowType is a 16-byte { id, w, h, pad } block.
	 * Any other layout (e.g. 3-int) freezes eglSwapBuffers. */
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

		/* eglTerminate is intentionally NOT called: Piglet cannot be
		 * re-initialized within a process. s_display is reused by GLimp_Init. */
	}

	Com_Printf("PS4 GL context shut down\n");
}

void GLimp_Minimize(void)
{
}

void GLimp_LogComment(char *comment)
{
}
