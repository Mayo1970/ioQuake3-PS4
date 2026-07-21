/* ps4_shaderbin.c -- capture/load of Piglet's native shader binary blobs.
 * See ps4_shaderbin.h / SHADER_PRECOMPILE_PLAN.md.
 *
 * PS4_LoadShaderBinary (below) ships in every PS4 build, debug and release --
 * it's the mechanism that lets release builds skip ShaccVSH entirely.
 * PS4_ShaderBin_Init/Dump (bottom, #ifdef PS4_DEBUG) are the capture side,
 * debug-only, kept permanently for capturing new shader content on future
 * flavors/mods.
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdbool.h>
#include <orbis/Pigletv2VSH.h>

#include "../renderercommon/tr_common.h"
#include "ps4_shaderbin.h"

/* Read straight from the PKG's own read-only mount -- no /data/ioq3/ copy,
 * no PS4_InstallFixes sync needed for this. /app0/ is read-only but that's
 * all PS4_LoadShaderBinary ever needs (fopen "rb"). Capture (below) still
 * needs a writable target, hence the separate /data/ dump dir. */
#define PS4_SHADERBIN_DIR  "/app0/fixes/shaderbin"
#define PS4_SHADERDUMP_DIR "/data/ioq3/shaderdump"

/* djb2 hash. Must match exactly between capture and load, or shipped blobs
 * never hit -- this is the single source of truth for both paths. */
static unsigned int PS4_ShaderBin_HashBuf(const char *buf, int len)
{
	unsigned int h = 5381;
	int i;
	for (i = 0; i < len; i++)
		h = ((h << 5) + h) + (unsigned char)buf[i];
	return h;
}

qboolean PS4_LoadShaderBinary(GLuint shader, const char *src, int srcLen, GLenum shaderType, unsigned int *outHash)
{
	char path[256];
	unsigned int hash;
	FILE *f;
	long size, blobSize;
	GLenum format;
	void *blob;

	(void)shaderType;

	hash = PS4_ShaderBin_HashBuf(src, srcLen);
	if (outHash)
		*outHash = hash;
	snprintf(path, sizeof(path), "%s/%08x.bin", PS4_SHADERBIN_DIR, hash);

	f = fopen(path, "rb");
	if (!f)
		return qfalse;

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size <= (long)sizeof(GLenum)) {
		fclose(f);
		return qfalse;
	}

	if (fread(&format, sizeof(format), 1, f) != 1) {
		fclose(f);
		return qfalse;
	}
	blobSize = size - (long)sizeof(GLenum);
	blob = malloc(blobSize);
	if (!blob) {
		fclose(f);
		return qfalse;
	}
	if ((long)fread(blob, 1, blobSize, f) != blobSize) {
		fclose(f);
		free(blob);
		return qfalse;
	}
	fclose(f);

	/* format is whatever glPigletGetShaderBinarySCE reported for this exact
	 * blob at capture time (0x9270 on this console/FW) -- more trustworthy
	 * than guessing, since it came from the driver itself. If hardware
	 * testing shows this rejects the blob, PacBrew's proven reference
	 * (SDL_render_gles2.c) hardcodes a literal 0 instead; try that next. */
	glShaderBinary(1, &shader, format, blob, (GLint)blobSize);
	free(blob);

	return qtrue;
}

#ifdef PS4_DEBUG

/* Signature confirmed by Phase 0 hardware logging: returns Piglet's own
 * Shader Binary container (magic 71 bc 91 e8, embedded GLSL source,
 * compiled microcode) for a shader object that has already been compiled. */
typedef void (*PFNGLPIGLETGETSHADERBINARYSCEPROC)(GLuint, GLsizei, GLsizei *, GLenum *, void *);

static PFNGLPIGLETGETSHADERBINARYSCEPROC s_glPigletGetShaderBinarySCE = NULL;
static qboolean s_initDone = qfalse;

void PS4_ShaderBin_Init(void)
{
	if (s_initDone)
		return;
	s_initDone = qtrue;

	s_glPigletGetShaderBinarySCE = (PFNGLPIGLETGETSHADERBINARYSCEPROC)eglGetProcAddress("glPigletGetShaderBinarySCE");
	mkdir(PS4_SHADERDUMP_DIR, 0755);
}

void PS4_ShaderBin_Dump(GLuint shader, const char *src, int srcLen, GLenum shaderType)
{
	static unsigned char buf[65536];
	unsigned int hash;
	char path[256];
	GLsizei length;
	GLenum format;
	FILE *f;
	struct stat st;
	const char *stageStr;

	PS4_ShaderBin_Init();

	if (!s_glPigletGetShaderBinarySCE)
		return;

	hash = PS4_ShaderBin_HashBuf(src, srcLen);
	snprintf(path, sizeof(path), "%s/%08x.bin", PS4_SHADERDUMP_DIR, hash);

	/* Already captured (this session or a previous one) -- skip. */
	if (stat(path, &st) == 0)
		return;

	length = 0;
	format = 0;
	s_glPigletGetShaderBinarySCE(shader, sizeof(buf), &length, &format, buf);
	if (length <= 0 || (size_t)length > sizeof(buf))
		return;

	f = fopen(path, "wb");
	if (!f)
		return;
	fwrite(&format, sizeof(format), 1, f);
	fwrite(buf, 1, length, f);
	fclose(f);

	stageStr = (shaderType == GL_VERTEX_SHADER) ? "vp" : "fp";

	f = fopen(PS4_SHADERDUMP_DIR "/manifest.txt", "a");
	if (f) {
		int previewLen = srcLen < 64 ? srcLen : 64;
		int i;
		fprintf(f, "%08x %s %d ", hash, stageStr, (int)length);
		for (i = 0; i < previewLen; i++) {
			char c = src[i];
			fputc((c == '\n' || c == '\r') ? ' ' : c, f);
		}
		fprintf(f, "\n");
		fclose(f);
	}

	Com_Printf("PS4: shader binary captured: %08x (%s, %d bytes, fmt=0x%x)\n",
	           hash, stageStr, (int)length, (unsigned int)format);
}

#endif /* PS4_DEBUG */
