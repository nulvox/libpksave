# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
#
# NDS toolchain: BlocksDS.
# Builds frontend-nds → /src/bin/pksave-nds.nds
# Compile-time flags: -DPKSAVE_GEN4 -DPKSAVE_GEN5
#
# BlocksDS Docker image: https://github.com/blocksds/sdk

# hadolint ignore=DL3007
FROM ghcr.io/blocksds/sdk:latest

WORKDIR /src

CMD ["make", "-C", "/src/frontend-nds", "all"]
