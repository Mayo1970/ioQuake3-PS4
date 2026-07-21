/* ps4_shaderbin.h -- capture/load of Piglet's native shader binary blobs.
 *
 * SHADER_PRECOMPILE_PLAN.md: eliminates the ShaccVSH runtime-compile
 * dependency by shipping precompiled per-stage shader binaries, dumped once
 * via glPigletGetShaderBinarySCE (confirmed present/working on this
 * console's FW 9.00 Piglet, Phase 0).
 *
 * Callers must include their own GL headers (GLuint/GLenum/GLsizei) before
 * this file.
 */
#ifndef PS4_SHADERBIN_H
#define PS4_SHADERBIN_H

/* Defined in ps4_glimp.c. True once libSceShaccVSH loaded successfully --
 * i.e. runtime glShaderSource/glCompileShader is usable as a fallback. */
qboolean PS4_ShaccAvailable(void);

/* Release + debug path. Call right before source-compiling a shader; on
 * qtrue a blob was found and handed to glShaderBinary (caller still checks
 * GL_COMPILE_STATUS the normal way). qfalse means no blob shipped for this
 * hash -- caller should fall back to source compile if PS4_ShaccAvailable().
 * *outHash is always set (even on qfalse) so a caller can name the missing
 * shader in an error message; pass NULL if not needed. */
qboolean PS4_LoadShaderBinary(GLuint shader, const char *src, int srcLen, GLenum shaderType, unsigned int *outHash);

#ifdef PS4_DEBUG
/* Capture path (debug builds only). Resolves the dump API and creates the
 * dump directory; safe to call redundantly, first call wins. */
void PS4_ShaderBin_Init(void);

/* Call right after a successful qglCompileShader/glCompileShader. Hashes
 * `src` the same way the runtime load path will, and — if not already
 * dumped — writes /data/ioq3/shaderdump/<hash>.bin plus a manifest.txt
 * line. No-op if the capture API didn't resolve. */
void PS4_ShaderBin_Dump(GLuint shader, const char *src, int srcLen, GLenum shaderType);
#endif /* PS4_DEBUG */

#endif /* PS4_SHADERBIN_H */
