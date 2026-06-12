#!/bin/bash
set -euo pipefail

# --- CONFIG ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
TARGET_NAME="blink"
USB_VID_PID="1209:be92"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

CLEAN=false
FLASH=false

show_help() {
    echo "Usage: ./build_and_flash.sh [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -c, --clean    Perform a clean build (remove build directory first)"
    echo "  -f, --flash    Flash the application via dfu-util"
    echo "  -h, --help     Show this help message"
}

parse_short_opts() {
    local flags="${1#-}"
    local i c
    for ((i = 0; i < ${#flags}; i++)); do
        c="${flags:$i:1}"
        case "$c" in
            c) CLEAN=true ;;
            f) FLASH=true ;;
            h) show_help; exit 0 ;;
            *)
                echo -e "${RED}Unknown option: -${c}${NC}"
                show_help
                exit 1
                ;;
        esac
    done
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean) CLEAN=true; shift ;;
        --flash) FLASH=true; shift ;;
        --help) show_help; exit 0 ;;
        -?*)
            parse_short_opts "$1"
            shift
            ;;
        *)
            echo -e "${RED}Unexpected argument: $1${NC}"
            show_help
            exit 1
            ;;
    esac
done

if [[ "$CLEAN" == true ]]; then
    echo -e "${YELLOW}=== Cleaning build directory ===${NC}"
    rm -rf "$BUILD_DIR"
fi

if [[ ! -d "$BUILD_DIR" ]]; then
    mkdir -p "$BUILD_DIR"
fi

echo -e "${YELLOW}=== Initializing CMake ===${NC}"
cd "$BUILD_DIR"
cmake -G "Unix Makefiles" "$SCRIPT_DIR"

echo -e "${YELLOW}=== Building ${TARGET_NAME} ===${NC}"
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
make -j"$JOBS"

echo -e "${GREEN}=== Build successful: ${TARGET_NAME} ===${NC}"
echo -e "Binary: ${BUILD_DIR}/${TARGET_NAME}.bin"

# Flash via dfu-util
if [[ "$FLASH" == true ]]; then
    BIN_FILE="${BUILD_DIR}/${TARGET_NAME}.bin"
    if [[ ! -f "$BIN_FILE" ]]; then
        echo -e "${RED}Binary not found: ${BIN_FILE}${NC}"
        exit 1
    fi

    echo -e "${YELLOW}=== Flashing via dfu-util ===${NC}"
    dfu-util -a 0 -d "$USB_VID_PID" -D "$BIN_FILE"
    echo -e "${GREEN}=== Flash successful ===${NC}"
fi
