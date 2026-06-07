# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
#
# Headless mGBA + Lua scripting harness for frontend-gba TDD.
# CMD is intentionally a no-op; the main thread wires `make test-gba`.
#
# What's inside:
#   - Ubuntu 24.04 runtime layer.
#   - `mgba-lua`: a tiny libmgba-linked binary built from this folder's
#     mgba-lua.c. It takes <rom> <script.lua> [frames] [--save <path>],
#     registers the Lua engine, optionally loads a sidecar .sav fixture
#     (temporary mode — writes do not bleed back), attaches the ROM to
#     the script context, runs the script, advances N frames (default
#     60), and exits cleanly. Required because upstream mGBA (verified
#     through 0.10.5 / master) does NOT expose a CLI flag for loading
#     Lua scripts — only the Qt GUI does.
#   - `/opt/mgba-test/smoke.gba`: a 196-byte valid GBA cartridge whose entrypoint
#     branches to itself, baked in at build time so the smoke test doesn't
#     depend on a real ROM artifact.
#
# Verified headless invocation (no SDL_VIDEODRIVER tricks needed — this
# binary never opens a window or audio device):
#   docker run --rm pksave-test-gba \
#       mgba-lua /opt/mgba-test/smoke.gba /opt/mgba-test/smoke.lua 60
#
# To drive a real frontend-gba build later, mount the repo and point at
# the artifact and a project-local script, e.g.:
#   docker run --rm -v "$(pwd):/src" pksave-test-gba \
#       mgba-lua /src/bin/pksave-gba.gba /src/frontend-gba/tests/foo.lua 120

# ── build stage ──────────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

# hadolint ignore=DL3008
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        git \
        libelf-dev \
        liblua5.4-dev \
        libminizip-dev \
        libpng-dev \
        libsqlite3-dev \
        pkg-config \
        python3 \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Pin a specific mGBA release for reproducibility.
# https://github.com/mgba-emu/mgba/releases
ARG MGBA_TAG=0.10.5
ENV MGBA_SRC=/opt/mgba-src
ENV MGBA_BUILD=/opt/mgba-build
ENV MGBA_PREFIX=/opt/mgba

RUN git clone --depth 1 --branch "${MGBA_TAG}" \
        https://github.com/mgba-emu/mgba.git "${MGBA_SRC}"

# Build libmgba with scripting/Lua enabled and every frontend / non-Lua
# optional dependency disabled. We can't use `-DLIBMGBA_ONLY=ON` here:
# that flag forces `DISABLE_DEPS=ON`, which causes mGBA's FindFeature.cmake
# to short-circuit *every* feature (including USE_LUA) to OFF — leaving
# libmgba.a without the Lua engine symbols. Turning off the frontends one
# by one keeps DISABLE_DEPS=OFF so the Lua find_feature() call actually
# resolves to liblua5.4-dev.
RUN cmake -S "${MGBA_SRC}" -B "${MGBA_BUILD}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${MGBA_PREFIX}" \
        -DBUILD_QT=OFF \
        -DBUILD_SDL=OFF \
        -DBUILD_SHARED=OFF \
        -DBUILD_STATIC=ON \
        -DBUILD_LTO=OFF \
        -DUSE_LUA=ON \
        -DENABLE_SCRIPTING=ON \
        -DUSE_DEBUGGERS=OFF \
        -DUSE_GDB_STUB=OFF \
        -DUSE_FFMPEG=OFF \
        -DUSE_DISCORD_RPC=OFF \
        -DUSE_EPOXY=OFF \
        -DUSE_LIBZIP=OFF \
        -DUSE_LZMA=OFF \
        -DBUILD_GL=OFF \
        -DBUILD_GLES2=OFF \
        -DBUILD_GLES3=OFF \
    && cmake --build "${MGBA_BUILD}" --target mgba --parallel \
    && cmake --install "${MGBA_BUILD}"

# Build the headless mgba-lua harness against the just-installed libmgba.
# We link libmgba.a statically and pull in its third-party shared deps so
# the runtime image only needs the small `lib*` packages installed below.
COPY mgba-lua.c /tmp/mgba-lua.c
RUN gcc -O2 -Wall -Wextra -std=gnu11 \
        -I"${MGBA_PREFIX}/include" \
        -L"${MGBA_PREFIX}/lib" \
        -o /usr/local/bin/mgba-lua /tmp/mgba-lua.c \
        -Wl,--whole-archive -lmgba -Wl,--no-whole-archive \
        -llua5.4 -lpng -lz -lsqlite3 -lelf -lminizip \
        -lpthread -lm -ldl

# Bake a valid 192-byte GBA cart (branch-to-self) into the image so the
# smoke test is self-contained.
COPY make-smoke-rom.py /tmp/make-smoke-rom.py
RUN mkdir -p /opt/mgba-test \
    && python3 /tmp/make-smoke-rom.py /opt/mgba-test/smoke.gba

COPY smoke.lua /opt/mgba-test/smoke.lua

# ── runtime stage ────────────────────────────────────────────────────────────
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# hadolint ignore=DL3008
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        liblua5.4-0 \
        libpng16-16t64 \
        libsqlite3-0 \
        libelf1t64 \
        libminizip1t64 \
        zlib1g \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /usr/local/bin/mgba-lua /usr/local/bin/mgba-lua
COPY --from=build /opt/mgba-test /opt/mgba-test

WORKDIR /src

# CMD is intentionally a no-op; the main thread wires `make test-gba` later
# to invoke `mgba-lua` against the artifact + test script of its choice.
# The smoke test (see header comment) overrides this CMD explicitly.
CMD ["sh", "-c", "echo 'pksave-test-gba: usage: mgba-lua <rom.gba> <script.lua> [frames]'"]
