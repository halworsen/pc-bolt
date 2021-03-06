#!/bin/sh

SCRIPT_DIR=$(dirname $(readlink -f $0))

[ ! -d "${PREFETCHER_FRAMEWORK}/m5" ] && {
        echo "Cannot locate m5 framework" >&2
        exit 1
}

cd ${PREFETCHER_FRAMEWORK}/m5

SCONS=$(which scons)
if (which scons2); then {
        SCONS=$(which scons2)
} fi
$SCONS -j2 NO_FAST_ALLOC=False EXTRAS="${SCRIPT_DIR}/../src" "${SCRIPT_DIR}/../build/ALPHA_SE/m5.opt"
