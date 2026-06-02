# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
#
# GBA toolchain: devkitARM + butano.
# Builds frontend-gba → /src/bin/pksave-gba.gba
# Compile-time flags: -DPKSAVE_GEN3

# hadolint ignore=DL3007
FROM devkitpro/devkitarm:latest

# Install build utilities not present in the base image.
# hadolint ignore=DL3008
RUN apt-get update && apt-get install -y --no-install-recommends \
        wget \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Pin a specific butano release for reproducibility.
# https://github.com/GValiente/butano/releases
ENV BUTANO_VERSION=18.5.0
ENV BUTANO_DIR=/opt/butano

RUN wget -q \
      "https://github.com/GValiente/butano/archive/refs/tags/${BUTANO_VERSION}.tar.gz" \
      -O /tmp/butano.tar.gz \
    && mkdir -p "${BUTANO_DIR}" \
    && tar -xzf /tmp/butano.tar.gz -C "${BUTANO_DIR}" --strip-components=1 \
    && rm /tmp/butano.tar.gz

# butano requires BUTANO_HOME to be set.
ENV BUTANO_HOME="${BUTANO_DIR}"

WORKDIR /src

CMD ["make", "-C", "/src/frontend-gba", "all"]
