/*
===========================================================================
Copyright (C) 2011 James Canete (use.less01@gmail.com)

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_extensions.c - extensions needed by the renderer not in sdl_glimp.c

#if !defined(__ORBIS__) && !defined(__PS4__)
#ifdef USE_INTERNAL_SDL_HEADERS
#	include "SDL.h"
#else
#	include <SDL.h>
#endif
#endif

#include "tr_local.h"
#include "tr_dsa.h"

void GLimp_InitExtraExtensions(void)
{
	char *extension;
	const char* result[3] = { "...ignoring %s\n", "...using %s\n", "...%s not found\n" };
	qboolean q_gl_version_at_least_3_0;
	qboolean q_gl_version_at_least_3_2;

	q_gl_version_at_least_3_0 = QGL_VERSION_ATLEAST( 3, 0 );
	q_gl_version_at_least_3_2 = QGL_VERSION_ATLEAST( 3, 2 );

	// Check if we need Intel graphics specific fixes.
	glRefConfig.intelGraphics = qfalse;
	if (strstr((char *)qglGetString(GL_RENDERER), "Intel"))
		glRefConfig.intelGraphics = qtrue;

	if (qglesMajorVersion)
	{
		glRefConfig.vaoCacheGlIndexType = GL_UNSIGNED_SHORT;
		glRefConfig.vaoCacheGlIndexSize = sizeof(unsigned short);
	}
	else
	{
		glRefConfig.vaoCacheGlIndexType = GL_UNSIGNED_INT;
		glRefConfig.vaoCacheGlIndexSize = sizeof(unsigned int);
	}

#if !defined(__ORBIS__) && !defined(__PS4__)
	// set DSA fallbacks (desktop only; PS4 uses GLDSA_* directly via qgl.h macros)
#define GLE(ret, name, ...) qgl##name = GLDSA_##name;
	QGL_EXT_direct_state_access_PROCS;
#undef GLE

	// GL function loader, based on https://gist.github.com/rygorous/16796a0c876cf8a5f542caddb55bce8a
#define GLE(ret, name, ...) qgl##name = (name##proc *) SDL_GL_GetProcAddress("gl" #name);
#endif

	//
	// OpenGL ES extensions
	//
	if (qglesMajorVersion)
	{
		if (!r_allowExtensions->integer)
			goto done;

#if defined(__ORBIS__) || defined(__PS4__)
		// PS4/Piglet: FBO functions are core in GLES2, no proc loading needed.
		// qgl* macros in qgl.h already map to gl* directly.
		glRefConfig.framebufferObject = !!r_ext_framebuffer_object->integer;
		glRefConfig.framebufferBlit = qfalse;       // GLES3-only, not available
		glRefConfig.framebufferMultisample = qfalse; // GLES3-only, not available

		qglGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &glRefConfig.maxRenderbufferSize);
		qglGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &glRefConfig.maxColorAttachments);

		ri.Printf(PRINT_ALL, "...using core GLES2 framebuffer objects\n");

		// Check extensions via glGetString since SDL is not available
		{
			const char *exts = (const char *)qglGetString(GL_EXTENSIONS);

			// GL_EXT_shadow_samplers
			if (exts && strstr(exts, "GL_EXT_shadow_samplers"))
			{
				glRefConfig.shadowSamplers = qtrue;
				ri.Printf(PRINT_ALL, "...using GL_EXT_shadow_samplers\n");
			}

			// GL_OES_standard_derivatives
			if (exts && strstr(exts, "GL_OES_standard_derivatives"))
			{
				glRefConfig.standardDerivatives = qtrue;
				ri.Printf(PRINT_ALL, "...using GL_OES_standard_derivatives\n");
			}

			// GL_OES_element_index_uint
			if (exts && strstr(exts, "GL_OES_element_index_uint"))
			{
				glRefConfig.vaoCacheGlIndexType = GL_UNSIGNED_INT;
				glRefConfig.vaoCacheGlIndexSize = sizeof(unsigned int);
				ri.Printf(PRINT_ALL, "...using GL_OES_element_index_uint\n");
			}

			// GL_OES_depth_texture (useful for shadow maps)
			if (exts && strstr(exts, "GL_OES_depth_texture"))
			{
				ri.Printf(PRINT_ALL, "...GL_OES_depth_texture available\n");
			}
		}

		// No occlusion queries on GLES2
		glRefConfig.occlusionQuery = qfalse;

		// No depth/stencil readback on GLES2 (GL_NV_read_depth not available)
		glRefConfig.readDepth = qfalse;
		glRefConfig.readStencil = qfalse;

		// No VAOs on base GLES2
		glRefConfig.vertexArrayObject = qfalse;

		// No texture float on base GLES2
		glRefConfig.textureFloat = qfalse;

		// No depth clamp on GLES2
		glRefConfig.depthClamp = qfalse;

		// No seamless cubemap on GLES2
		glRefConfig.seamlessCubeMap = qfalse;

		// No memory info extensions
		glRefConfig.memInfo = MI_NONE;

		// No texture compression (RGTC/BPTC are desktop-only)
		glRefConfig.textureCompression = TCR_NONE;
		glRefConfig.swizzleNormalmap = qfalse;

		// DSA fallbacks are already set above (before the GLES block)
		glRefConfig.directStateAccess = qfalse;

#else /* !__ORBIS__ -- standard GLES path with SDL */
		extension = "GL_EXT_occlusion_query_boolean";
		if (qglesMajorVersion >= 3 || SDL_GL_ExtensionSupported(extension))
		{
			glRefConfig.occlusionQuery = qtrue;
			glRefConfig.occlusionQueryTarget = GL_ANY_SAMPLES_PASSED;

			if (qglesMajorVersion >= 3) {
				QGL_ARB_occlusion_query_PROCS;
			} else {
				// GL_EXT_occlusion_query_boolean uses EXT suffix
#undef GLE
#define GLE(ret, name, ...) qgl##name = (name##proc *) SDL_GL_GetProcAddress("gl" #name "EXT");

				QGL_ARB_occlusion_query_PROCS;

#undef GLE
#define GLE(ret, name, ...) qgl##name = (name##proc *) SDL_GL_GetProcAddress("gl" #name);
			}

			ri.Printf(PRINT_ALL, result[glRefConfig.occlusionQuery], extension);
		}
		else
		{
			ri.Printf(PRINT_ALL, result[2], extension);
		}

		// GL_NV_read_depth
		extension = "GL_NV_read_depth";
		if (SDL_GL_ExtensionSupported(extension))
		{
			glRefConfig.readDepth = qtrue;
			ri.Printf(PRINT_ALL, result[glRefConfig.readDepth], extension);
		}
		else
		{
			ri.Printf(PRINT_ALL, result[2], extension);
		}

		// GL_NV_read_stencil
		extension = "GL_NV_read_stencil";
		if (SDL_GL_ExtensionSupported(extension))
		{
			glRefConfig.readStencil = qtrue;
			ri.Printf(PRINT_ALL, result[glRefConfig.readStencil], extension);
		}
		else
		{
			ri.Printf(PRINT_ALL, result[2], extension);
		}

		// GL_EXT_shadow_samplers
		extension = "GL_EXT_shadow_samplers";
		if (qglesMajorVersion >= 3 || SDL_GL_ExtensionSupported(extension))
		{
			glRefConfig.shadowSamplers = qtrue;
			ri.Printf(PRINT_ALL, result[glRefConfig.shadowSamplers], extension);
		}
		else
		{
			ri.Printf(PRINT_ALL, result[2], extension);
		}

		// GL_OES_standard_derivatives
		extension = "GL_OES_standard_derivatives";
		if (qglesMajorVersion >= 3 || SDL_GL_ExtensionSupported(extension))
		{
			glRefConfig.standardDerivatives = qtrue;
			ri.Printf(PRINT_ALL, result[glRefConfig.standardDerivatives], extension);
		}
		else
		{
			ri.Printf(PRINT_ALL, result[2], extension);
		}

		// GL_OES_element_index_uint
		extension = "GL_OES_element_index_uint";
		if (qglesMajorVersion >= 3 || SDL_GL_ExtensionSupported(extension))
		{
			glRefConfig.vaoCacheGlIndexType = GL_UNSIGNED_INT;
			glRefConfig.vaoCacheGlIndexSize = sizeof(unsigned int);
			ri.Printf(PRINT_ALL, result[1], extension);
		}
		else
		{
			ri.Printf(PRINT_ALL, result[2], extension);
		}
#endif /* __ORBIS__ */

		goto done;
	}

#if !defined(__ORBIS__) && !defined(__PS4__)
	// OpenGL 1.5 - GL_ARB_occlusion_query
	glRefConfig.occlusionQuery = qtrue;
	glRefConfig.occlusionQueryTarget = GL_SAMPLES_PASSED;
	QGL_ARB_occlusion_query_PROCS;

	// OpenGL 3.0 - GL_ARB_framebuffer_object
	extension = "GL_ARB_framebuffer_object";
	glRefConfig.framebufferObject = qfalse;
	glRefConfig.framebufferBlit = qfalse;
	glRefConfig.framebufferMultisample = qfalse;
	if (q_gl_version_at_least_3_0 || SDL_GL_ExtensionSupported(extension))
	{
		glRefConfig.framebufferObject = !!r_ext_framebuffer_object->integer;
		glRefConfig.framebufferBlit = qtrue;
		glRefConfig.framebufferMultisample = qtrue;

		qglGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &glRefConfig.maxRenderbufferSize);
		qglGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &glRefConfig.maxColorAttachments);

		QGL_ARB_framebuffer_object_PROCS;

		ri.Printf(PRINT_ALL, result[glRefConfig.framebufferObject], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	// OpenGL 3.0 - GL_ARB_vertex_array_object
	extension = "GL_ARB_vertex_array_object";
	glRefConfig.vertexArrayObject = qfalse;
	if (q_gl_version_at_least_3_0 || SDL_GL_ExtensionSupported(extension))
	{
		if (q_gl_version_at_least_3_0)
		{
			// force VAO, core context requires it
			glRefConfig.vertexArrayObject = qtrue;
		}
		else
		{
			glRefConfig.vertexArrayObject = !!r_arb_vertex_array_object->integer;
		}

		QGL_ARB_vertex_array_object_PROCS;

		ri.Printf(PRINT_ALL, result[glRefConfig.vertexArrayObject], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	// OpenGL 3.0 - GL_ARB_texture_float
	extension = "GL_ARB_texture_float";
	glRefConfig.textureFloat = qfalse;
	if (q_gl_version_at_least_3_0 || SDL_GL_ExtensionSupported(extension))
	{
		glRefConfig.textureFloat = !!r_ext_texture_float->integer;

		ri.Printf(PRINT_ALL, result[glRefConfig.textureFloat], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	// OpenGL 3.2 - GL_ARB_depth_clamp
	extension = "GL_ARB_depth_clamp";
	glRefConfig.depthClamp = qfalse;
	if (q_gl_version_at_least_3_2 || SDL_GL_ExtensionSupported(extension))
	{
		glRefConfig.depthClamp = qtrue;

		ri.Printf(PRINT_ALL, result[glRefConfig.depthClamp], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	// OpenGL 3.2 - GL_ARB_seamless_cube_map
	extension = "GL_ARB_seamless_cube_map";
	glRefConfig.seamlessCubeMap = qfalse;
	if (q_gl_version_at_least_3_2 || SDL_GL_ExtensionSupported(extension))
	{
		glRefConfig.seamlessCubeMap = !!r_arb_seamless_cube_map->integer;

		ri.Printf(PRINT_ALL, result[glRefConfig.seamlessCubeMap], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	glRefConfig.memInfo = MI_NONE;

	// GL_NVX_gpu_memory_info
	extension = "GL_NVX_gpu_memory_info";
	if( SDL_GL_ExtensionSupported( extension ) )
	{
		glRefConfig.memInfo = MI_NVX;

		ri.Printf(PRINT_ALL, result[1], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	// GL_ATI_meminfo
	extension = "GL_ATI_meminfo";
	if( SDL_GL_ExtensionSupported( extension ) )
	{
		if (glRefConfig.memInfo == MI_NONE)
		{
			glRefConfig.memInfo = MI_ATI;

			ri.Printf(PRINT_ALL, result[1], extension);
		}
		else
		{
			ri.Printf(PRINT_ALL, result[0], extension);
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	glRefConfig.textureCompression = TCR_NONE;

	// GL_ARB_texture_compression_rgtc
	extension = "GL_ARB_texture_compression_rgtc";
	if (SDL_GL_ExtensionSupported(extension))
	{
		qboolean useRgtc = r_ext_compressed_textures->integer >= 1;

		if (useRgtc)
			glRefConfig.textureCompression |= TCR_RGTC;

		ri.Printf(PRINT_ALL, result[useRgtc], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	glRefConfig.swizzleNormalmap = r_ext_compressed_textures->integer && !(glRefConfig.textureCompression & TCR_RGTC);

	// GL_ARB_texture_compression_bptc
	extension = "GL_ARB_texture_compression_bptc";
	if (SDL_GL_ExtensionSupported(extension))
	{
		qboolean useBptc = r_ext_compressed_textures->integer >= 2;

		if (useBptc)
			glRefConfig.textureCompression |= TCR_BPTC;

		ri.Printf(PRINT_ALL, result[useBptc], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	// GL_EXT_direct_state_access
	extension = "GL_EXT_direct_state_access";
	glRefConfig.directStateAccess = qfalse;
	if (SDL_GL_ExtensionSupported(extension))
	{
		glRefConfig.directStateAccess = !!r_ext_direct_state_access->integer;

		// QGL_*_PROCS becomes several functions, do not remove {}
		if (glRefConfig.directStateAccess)
		{
			QGL_EXT_direct_state_access_PROCS;
		}

		ri.Printf(PRINT_ALL, result[glRefConfig.directStateAccess], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}
#endif /* !__ORBIS__ && !__PS4__ */

done:

	// Determine GLSL version
	if (1)
	{
		char version[256], *version_p;

		Q_strncpyz(version, (char *)qglGetString(GL_SHADING_LANGUAGE_VERSION), sizeof(version));

		// Skip leading text such as "OpenGL ES GLSL ES "
		version_p = version;
		while ( *version_p && !isdigit( *version_p ) )
		{
			version_p++;
		}

		// FORCED for PS4/GLES2 compatibility
		glRefConfig.glslMajorVersion = 1;
		glRefConfig.glslMinorVersion = 0;

		ri.Printf(PRINT_ALL, "...using GLSL version %s\n", version);
	}

#undef GLE
}
