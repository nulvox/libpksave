# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
#
# GBC toolchain: gbdk-2020.
# Builds frontend-gbc → /src/bin/pksave-gbc.gbc
# Compile-time flags: -DPKSAVE_GEN1 -DPKSAVE_GEN2

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        make \
        wget \
        ca-certificates \
        python3 \
    && rm -rf /var/lib/apt/lists/*

# Pin gbdk-2020 release.
# Check https://github.com/gbdk-2020/gbdk-2020/releases for latest.
ENV GBDK_VERSION=4.3.0
ENV GBDK_DIR=/opt/gbdk

RUN wget -q \
      "https://github.com/gbdk-2020/gbdk-2020/releases/download/${GBDK_VERSION}/gbdk-linux64.tar.gz" \
      -O /tmp/gbdk.tar.gz \
    && mkdir -p "${GBDK_DIR}" \
    && tar -xzf /tmp/gbdk.tar.gz -C "${GBDK_DIR}" --strip-components=1 \
    && rm /tmp/gbdk.tar.gz

ENV PATH="${GBDK_DIR}/bin:${PATH}"

WORKDIR /src

CMD ["make", "-C", "/src/frontend-gbc", "all"]
