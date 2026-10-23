#!/usr/bin/env bash
# Builds and runs the host self-test in a gcc container (no toolchain needed on the machine).
# Usage: test/host/run.sh [-v]      (-v prints the session log and every frame)
set -euo pipefail
cd "$(dirname "$0")/../.."
ROOT="$(pwd -W 2>/dev/null || pwd)"
export MSYS_NO_PATHCONV=1
docker run --rm -v "${ROOT}:/w" -w /w gcc:14 bash -c "
  g++ -std=c++17 -O1 -Wall -Wextra -Wno-unused-parameter -I src -I test/host \
    src/isotp/iso_tp.cpp src/uds/uds_mapper.cpp src/uds/uds_definitions.cpp \
    src/message_translator.cpp src/translator_table.cpp src/isf_broadcast.cpp src/services/isf_service.cpp \
    test/host/fake_bus.cpp test/host/fake_twai_wrapper.cpp test/host/host_platform.cpp test/host/selftest.cpp \
    -o /tmp/selftest && /tmp/selftest $*"
