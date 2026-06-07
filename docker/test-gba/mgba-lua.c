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
 */

#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/log.h>
#include <mgba/core/scripting.h>
#include <mgba/script/context.h>
#include <mgba-util/table.h>
#include <mgba-util/vfs.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	 *    stdout without needing a full mCoreConfig round-trip. */
	struct mStandardLogger logger = {0};
	mStandardLoggerInit(&logger);
	logger.logToStdout = true;
	logger.d.filter->defaultLevels = mLOG_ALL;
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
	fflush(stdout);
	return 0;
}
