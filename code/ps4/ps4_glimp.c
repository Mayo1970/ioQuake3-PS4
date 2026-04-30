/*
 * ps4_glimp.c - PS4 Piglet/EGL OpenGL ES 2.0 context management
 *
 * Replaces code/sdl/sdl_glimp.c for the PS4 platform.
 * Uses Sony's Piglet library (GLES2/EGL) for GPU rendering.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <orbis/Pigletv2VSH.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>

#include "../renderercommon/tr_common.h"
#include "../sys/sys_local.h"

// GLES version globals used by the renderer (declared extern in qgl.h).
// Desktop GL sets these in sdl_glimp.c; PS4 sets them here.
int qglMajorVersion = 0;
int qglMinorVersion = 0;
int qglesMajorVersion = 2;
int qglesMinorVersion = 0;

static EGLDisplay s_display = EGL_NO_DISPLAY;
static EGLSurface s_surface = EGL_NO_SURFACE;
static EGLContext  s_context = EGL_NO_CONTEXT;

static int s_pigletModuleId = -1;
static int s_shaccModuleId = -1;

/*
 * PS4_LoadPigletModules
 *
 * Load the Piglet GLES2 library and shader compiler at runtime.
 * Matches SM64 PS4 port approach: /data/ path first, no PrecompiledShaders,
 * no scePigletSetConfigurationVSH.
 */
static qboolean PS4_LoadPigletModules(void)
{
	// SM64 loads Piglet and ShaccVSH from hardcoded /data/ path.
	// The early test in main() already loaded them. If the modules
	// are already loaded, sceKernelLoadStartModule returns the existing
	// handle (or an "already loaded" error that is >= 0).
	// We try loading again here in case the early test was removed.

	s_pigletModuleId = sceKernelLoadStartModule(
		"/data/self/system/common/lib/libScePigletv2VSH.sprx",
		0, NULL, 0, NULL, NULL);
	Com_Printf("  Piglet (/data/): 0x%08X\n", s_pigletModuleId);

	if (s_pigletModuleId < 0) {
		// Fallback: try sandbox path
		const char *sw = sceKernelGetFsSandboxRandomWord();
		char path[256];
		snprintf(path, sizeof(path), "/%s/common/lib/libScePigletv2VSH.sprx", sw);
		s_pigletModuleId = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, NULL);
		Com_Printf("  Piglet (sandbox): 0x%08X\n", s_pigletModuleId);
	}

	if (s_pigletModuleId < 0) {
		// Last resort: bundled in app
		s_pigletModuleId = sceKernelLoadStartModule(
			"/app0/sce_module/libScePigletv2VSH.sprx",
			0, NULL, 0, NULL, NULL);
		Com_Printf("  Piglet (app0): 0x%08X\n", s_pigletModuleId);
	}

	if (s_pigletModuleId < 0) {
		Com_Printf("ERROR: Failed to load Piglet module: 0x%08X\n", s_pigletModuleId);
		return qfalse;
	}

	// SM64 does NOT load PrecompiledShaders -- skip it entirely.
	// (Loading it may interfere with Piglet initialization.)

	// Load ShaccVSH (runtime GLSL compiler)
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

/*
 * GLimp_Init
 *
 * Initialize Piglet, EGL, and create a GLES2 rendering context.
 */
void GLimp_Init(qboolean fixedFunction)
{
	EGLint major, minor;
	EGLConfig egl_config;
	EGLint num_configs;
	int width, height;

	Com_Printf("Initializing PS4 OpenGL ES 2.0 (Piglet)...\n");

	// Load Piglet and shader modules
	if (!PS4_LoadPigletModules()) {
		Com_Error(ERR_FATAL, "GLimp_Init: Failed to load Piglet modules");
		return;
	}

	// Determine resolution
	// PS4 requires standard display resolutions for Piglet to swap properly!
	// Force 1920x1080.
	width = 1920;
	height = 1080;
	ri.Cvar_Set("r_customwidth", "1920");
	ri.Cvar_Set("r_customheight", "1080");
	ri.Cvar_Set("r_mode", "-1"); // Custom mode

	// Shadow FBOs return GL_FRAMEBUFFER_UNSUPPORTED on Piglet; disable both
	// entity shadows (cg_shadows) and sun shadow maps (r_sunShadows).
	ri.Cvar_Set("cg_shadows", "0");
	ri.Cvar_Set("r_sunShadows", "0");

	// Configure Piglet (OpenOrbis sample values).
	// IMPORTANT: Piglet must NOT be in sce_module/ (auto-loaded at startup
	// would create an unconfigured instance). It must be loaded at runtime
	// via sceKernelLoadStartModule, then configured here.
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

	// Get EGL display
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

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		Com_Error(ERR_FATAL, "GLimp_Init: eglBindAPI failed");
		return;
	}

	// Choose EGL config
	// NOTE: Depth/stencil are 0 here -- renderergl2 uses FBOs for depth/stencil.
	// Both the OpenOrbis Piglet sample and OsirizX SM64 port request 0/0.
	// Requesting 24/8 may fail on Piglet (no matching EGL config).
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

	// Create window surface
	// SM64 uses SceWindow = { 0, width, height } (3 ints, no padding).
	// OpenOrbis uses OrbisPglWindow = { uID, uWidth, uHeight, uPadding }.
	// Use a simple struct to match SM64's proven layout exactly.
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

	// Create GLES2 context
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

	// Vsync: 0 = off, 1 = on
	eglSwapInterval(s_display, 0);

	// Fill in glconfig
	glConfig.vidWidth = width;
	glConfig.vidHeight = height;
	glConfig.colorBits = 32;
	glConfig.depthBits = 0;  // Default framebuffer has no depth; FBOs provide their own
	glConfig.stencilBits = 0;
	glConfig.isFullscreen = qtrue;
	glConfig.windowAspect = (float)width / (float)height;
	glConfig.stereoEnabled = qfalse;
	glConfig.smpActive = qfalse;
	glConfig.textureCompression = TC_NONE; // No S3TC/DXT on GLES2

	// Piglet GLSL version string is "OpenGL ES GLSL ES 2.0" 
	// (which parses to major 2, minor 0 in tr_extensions.c).
	// We want it to be 1.00 for GLSL ES 1.00 compatibility.

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

	// Initialize input now that the GL context and window are live.
	// SDL glimp calls ri.IN_Init(SDL_window) here; we pass NULL (no window handle needed on PS4).
	ri.IN_Init(NULL);
}

/*
 * GLimp_EndFrame
 *
 * Swap buffers to present the rendered frame.
 */
void GLimp_EndFrame(void)
{
	if (s_display != EGL_NO_DISPLAY && s_surface != EGL_NO_SURFACE) {
		eglSwapBuffers(s_display, s_surface);
	}
}

/*
 * GLimp_Shutdown
 *
 * Clean up EGL resources.
 */
void GLimp_Shutdown(void)
{
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

		eglTerminate(s_display);
		s_display = EGL_NO_DISPLAY;
	}

	Com_Printf("PS4 GL context shut down\n");
}

/*
 * GLimp_Minimize - No-op on PS4 (always fullscreen)
 */
void GLimp_Minimize(void)
{
}

/*
 * GLimp_LogComment - Debug logging for GL operations
 */
void GLimp_LogComment(char *comment)
{
}
