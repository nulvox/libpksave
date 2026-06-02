# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
#
# Test container: gcc + Unity.
# Compiles and runs libpksave Unity tests with all five PKSAVE_GENx flags.
# Volume-mounts the repo root at /src; writes no build output to /src/bin.

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc \
        make \
        wget \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Pin a specific Unity release for reproducibility.
# https://github.com/ThrowTheSwitch/Unity/releases
ENV UNITY_VERSION=2.6.0
ENV UNITY_DIR=/opt/unity/src

RUN mkdir -p "${UNITY_DIR}" \
    && BASE="https://raw.githubusercontent.com/ThrowTheSwitch/Unity/v${UNITY_VERSION}/src" \
    && wget -q "${BASE}/unity.h"            -O "${UNITY_DIR}/unity.h" \
    && wget -q "${BASE}/unity_internals.h"  -O "${UNITY_DIR}/unity_internals.h" \
    && wget -q "${BASE}/unity.c"            -O "${UNITY_DIR}/unity.c"

WORKDIR /src

CMD ["make", "-C", "/src/libpksave", "test", "UNITY_DIR=/opt/unity/src"]
