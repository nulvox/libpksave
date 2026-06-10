/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * mgba-lua: minimal headless mGBA + Lua harness.
 *
 * The upstream SDL frontend has no `--script` flag; Lua scripting is wired
 * into the Qt GUI only. This binary is the missing CLI: it embeds libmgba,
 * registers the Lua engine, attaches a ROM, runs a script, advances frames,
 * and exits.
 *
 * Usage:
 *   mgba-lua <rom.gba> <script.lua> [frames] [--save <path>]
 *
 * The optional `--save <path>` flag loads a sidecar .sav fixture into the
 * core's save backend before the initial reset (mirrors the SDL frontend's
 * mCoreAutoloadSave step). The save is loaded as `temporary` so writes do
 * NOT bleed back to the host file — fixture injection only. A flag is used
 * (instead of a 4th positional) so callers can pass a save without first
 * having to also specify a frames count.
 *
 * Conventions:
 *   - `console:log(...)` from Lua writes to stdout via the mGBA logger.
 *   - `emu` global exposes the running mCore (frame counter, reset, ...).
 *   - `callbacks` global accepts callbacks like `frame`, `start`, `stop`.
 *   - `frames` (default 60) controls how many frames the emulator runs
 *     after the script loads. A frame callback that calls os.exit() can
 *     end the run earlier.
 *   - `fb_dump_ppm(path)` is a custom Lua global (binding defined below)
 *     that writes the current GBA framebuffer to a binary PPM (P6) file.
 *     Used by the dirty-indicator regression test; see helpers.lua's
 *     `M.fb_dump(path)` wrapper for callers.
 */

#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/interface.h>
#include <mgba/core/log.h>
#include <mgba/core/scripting.h>
#include <mgba/script/context.h>
#include <mgba/script/macros.h>
#include <mgba/script/types.h>
#include <mgba-util/table.h>
#include <mgba-util/vfs.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Framebuffer dump support.
 *
 * The GBA renderer needs a destination buffer or its dummy renderer drops every
 * scanline on the floor; without that, `core->getPixels` returns NULL/garbage
 * and the built-in `emu:screenshot` SEGVs trying to PNG-encode it. We allocate
 * a 240×160×4-byte buffer (BGRA, BYTES_PER_PIXEL=4 per the libmgba default
 * build) once at startup, hand it to the core via `setVideoBuffer` BEFORE the
 * initial reset (so the reset path swaps the dummy renderer for the software
 * one — see src/gba/core.c:_GBACoreReset), and free it on shutdown.
 *
 * We then expose a custom Lua global `fb_dump_ppm(path)` that writes the
 * current framebuffer as a binary PPM (P6) file. PPM is chosen over PNG because:
 *   - the harness's Python helpers run on the bare stdlib (no Pillow / no PNG
 *     decoder), and a PPM parser is ~10 lines of struct.unpack
 *   - emu:screenshot still works for human-readable artefacts when needed
 * Color layout in the BGRA buffer is M_COLOR_RED=0xFF in byte 0, GREEN in byte
 * 1, BLUE in byte 2, ALPHA in byte 3 (see mgba/core/interface.h:21); we copy
 * bytes 0..2 of each pixel into the PPM's RGB triplets unchanged.
 */
static struct mCore* g_core = NULL;
static color_t* g_videoBuffer = NULL;
static unsigned g_videoWidth = 0;
static unsigned g_videoHeight = 0;

static int32_t fb_dump_ppm(const char* path) {
	if (!g_core || !g_videoBuffer) {
		return -1;
	}
	const void* pixels = NULL;
	size_t stride = 0;
	g_core->getPixels(g_core, &pixels, &stride);
	if (!pixels) {
		return -2;
	}
	FILE* f = fopen(path, "wb");
	if (!f) {
		return -3;
	}
	if (fprintf(f, "P6\n%u %u\n255\n", g_videoWidth, g_videoHeight) < 0) {
		fclose(f);
		return -4;
	}
	const uint8_t* row = (const uint8_t*) pixels;
	for (unsigned y = 0; y < g_videoHeight; ++y) {
		for (unsigned x = 0; x < g_videoWidth; ++x) {
			const uint8_t* px = row + x * 4;
			uint8_t rgb[3] = { px[0], px[1], px[2] };
			if (fwrite(rgb, 1, 3, f) != 3) {
				fclose(f);
				return -5;
			}
		}
		row += stride * 4;
	}
	if (fclose(f) != 0) {
		return -6;
	}
	return 0;
}

mSCRIPT_BIND_FUNCTION(boundFbDumpPpm, S32, fb_dump_ppm, 1, CHARP, path);

/* mScriptContextLoadFile only *loads* — Qt's ScriptingController calls
 * engine->run() afterwards. Do the same here by looking up the named engine.
 */
static bool loadAndRunScript(struct mScriptContext* sc, const char* path, const char* engineName) {
	struct mScriptEngineContext* engine = HashTableLookup(&sc->engines, engineName);
	if (!engine) {
		fprintf(stderr, "mgba-lua: script engine %s not registered\n", engineName);
		return false;
	}
	struct VFile* vf = VFileOpen(path, O_RDONLY);
	if (!vf) {
		fprintf(stderr, "mgba-lua: cannot open %s: %s\n", path, strerror(errno));
		return false;
	}
	bool ok = engine->load(engine, path, vf) && engine->run(engine);
	if (!ok) {
		const char* err = engine->getError ? engine->getError(engine) : NULL;
		fprintf(stderr, "mgba-lua: script error: %s\n", err ? err : "(unknown)");
	}
	vf->close(vf);
	return ok;
}

static const char* USAGE =
    "Usage: mgba-lua <rom.gba> <script.lua> [frames] [--save <path>]\n";

int main(int argc, char** argv) {
	if (argc < 3) {
		fputs(USAGE, stderr);
		return 2;
	}
	const char* romPath = argv[1];
	const char* scriptPath = argv[2];
	long frames = 60;
	const char* savePath = NULL;
	bool framesSet = false;

	/* Parse trailing args: an optional positional `frames` and an optional
	 * `--save <path>` flag. We accept them in either order so callers don't
	 * have to remember a specific layout. */
	for (int i = 3; i < argc; ++i) {
		if (strcmp(argv[i], "--save") == 0) {
			if (i + 1 >= argc) {
				fputs(USAGE, stderr);
				return 2;
			}
			savePath = argv[++i];
		} else if (!framesSet) {
			char* end = NULL;
			errno = 0;
			frames = strtol(argv[i], &end, 10);
			if (errno || !end || *end || frames < 0) {
				fputs(USAGE, stderr);
				return 2;
			}
			framesSet = true;
		} else {
			fputs(USAGE, stderr);
			return 2;
		}
	}

	/* Logger: writes script `console:log` output (and any mGBA logs) to stdout.
	 *  - Zero-initialise the struct: mStandardLoggerInit only sets ->d.log and
	 *    ->d.filter; ->logToStdout and ->logFile come from stack garbage and
	 *    cause a SEGV on the `logFile->write(...)` virtual call otherwise.
	 *  - mStandardLoggerInit allocates a filter with defaultLevels=0 (would
	 *    block everything); widen it to mLOG_ALL so console:log lines reach
	 *    stdout without needing a full mCoreConfig round-trip.
	 *  - mGBA's "GBA Debug" category (where butano's BN_LOG and any direct
	 *    writes to the mGBA debug-print MMIO surface) has its filter level
	 *    set per-category and is NOT promoted by defaultLevels alone — by
	 *    default it sits at mLOG_FATAL only.  Editor scenes drop "scene: OK"
	 *    breadcrumbs through BN_LOG so the lua tests can grep them; without
	 *    bumping gba.debug to mLOG_ALL those lines never reach stdout. */
	struct mStandardLogger logger = {0};
	mStandardLoggerInit(&logger);
	logger.logToStdout = true;
	logger.d.filter->defaultLevels = mLOG_ALL;
	mLogFilterSet(logger.d.filter, "gba.debug", mLOG_ALL);
	mLogSetDefaultLogger(&logger.d);

	/* Build a core from the ROM and initialise it. */
	struct mCore* core = mCoreFind(romPath);
	if (!core) {
		fprintf(stderr, "mgba-lua: could not find core for %s\n", romPath);
		mStandardLoggerDeinit(&logger);
		return 1;
	}
	if (!core->init(core)) {
		fprintf(stderr, "mgba-lua: core init failed\n");
		mStandardLoggerDeinit(&logger);
		return 1;
	}
	mCoreInitConfig(core, NULL);
	mCoreConfigSetDefaultIntValue(&core->config, "logToStdout", 1);
	mCoreLoadConfig(core);

	if (!mCorePreloadFile(core, romPath)) {
		fprintf(stderr, "mgba-lua: failed to load ROM %s\n", romPath);
		core->deinit(core);
		mStandardLoggerDeinit(&logger);
		return 1;
	}

	/* Allocate a video buffer and hand it to the core *before* the initial
	 * reset. The GBA core's reset path checks `gbacore->renderer.outputBuffer`
	 * and only installs the software renderer if it's non-NULL — without this
	 * step the dummy renderer stays bound and `core->getPixels` is a no-op,
	 * which is also what makes `emu:screenshot` SEGV. Buffer must live until
	 * after the final core->deinit, so we free at the very end of main(). */
	core->desiredVideoDimensions(core, &g_videoWidth, &g_videoHeight);
	g_videoBuffer = calloc((size_t) g_videoWidth * g_videoHeight, sizeof(color_t));
	if (!g_videoBuffer) {
		fprintf(stderr, "mgba-lua: failed to allocate %ux%u video buffer\n",
		        g_videoWidth, g_videoHeight);
		core->deinit(core);
		mStandardLoggerDeinit(&logger);
		return 1;
	}
	core->setVideoBuffer(core, g_videoBuffer, g_videoWidth);
	g_core = core;

	/* Optional sidecar save load. Must run AFTER mCorePreloadFile (so the
	 * core knows the cart's save type) and BEFORE core->reset() (so the
	 * fresh SRAM/FLASH region is populated before code starts running).
	 * We pass `temporary=true` so writes during the run don't bleed back
	 * to the host fixture file — see mGBA src/core/core.c:mCoreLoadSaveFile.
	 * If the cart's declared save size is smaller than the sidecar, only
	 * the leading bytes that fit get used; that's fine for fixture work. */
	if (savePath) {
		if (!mCoreLoadSaveFile(core, savePath, true)) {
			fprintf(stderr, "mgba-lua: failed to load save %s\n", savePath);
			core->deinit(core);
			mStandardLoggerDeinit(&logger);
			return 1;
		}
	}

	/* Set up the script context: stdlib + sockets + Lua engine. */
	struct mScriptContext sc;
	mScriptContextInit(&sc);
	mScriptContextAttachStdlib(&sc);
	mScriptContextAttachSocket(&sc);
	mScriptContextRegisterEngines(&sc);
	mScriptContextAttachLogger(&sc, &logger.d);
	mScriptContextAttachCore(&sc, core);

	/* Expose the framebuffer-dump function as a Lua global. The bound value
	 * is `const`, but mScriptContextSetGlobal's weakref dance only Ref/Deref
	 * the underlying storage — both are no-ops for mSCRIPT_VALUE_UNREF (refs
	 * == -1) values produced by mSCRIPT_BIND_FUNCTION, so the const cast is
	 * safe. Lua callers see a global `fb_dump_ppm(path)` returning 0 on
	 * success or a negative error code (see fb_dump_ppm above). */
	mScriptContextSetGlobal(&sc, "fb_dump_ppm",
	                        (struct mScriptValue*) &boundFbDumpPpm);

	/* Reset the core so `emu` reflects a fresh boot. */
	core->reset(core);

	/* Load and run the Lua script (load + run, like the Qt frontend does). */
	if (!loadAndRunScript(&sc, scriptPath, "lua")) {
		mScriptContextDetachCore(&sc);
		mScriptContextDeinit(&sc);
		core->deinit(core);
		mStandardLoggerDeinit(&logger);
		return 1;
	}

	/* Drive the emulator forward and fire the `frame` callback each iteration. */
	for (long i = 0; i < frames; ++i) {
		core->runFrame(core);
		mScriptContextTriggerCallback(&sc, "frame");
	}

	mScriptContextTriggerCallback(&sc, "shutdown");
	mScriptContextDetachCore(&sc);
	mScriptContextDeinit(&sc);
	core->deinit(core);
	mStandardLoggerDeinit(&logger);
	free(g_videoBuffer);
	g_videoBuffer = NULL;
	g_core = NULL;
	fflush(stdout);
	return 0;
}
