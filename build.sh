#!/bin/sh
# ./build.sh            Switch 1, blacklist-check.nro (devkitpro/devkita64)
# ./build.sh switch2    Switch 2, blacklist-check-switch2.nro (kaiHEN 0.0.4 SDK)
# Anything after that goes to make, e.g. ./build.sh switch2 clean
#
# The Switch 2 image is ounce-sdk:0.0.4, built from ../kaiHEN-0.0.4/sdk if it's
# missing. It's tagged separately so the 0.0.1 ounce-sdk:latest image that
# no-more-nag uses stays as it is.
set -e
cd "$(dirname "$0")"
platform=switch
image=devkitpro/devkita64:latest
case "${1:-}" in
switch) shift ;;
switch2)
    shift
    platform=switch2
    image=ounce-sdk:0.0.4
    if ! docker image inspect "$image" >/dev/null 2>&1; then
        docker build -t "$image" ../kaiHEN-0.0.4/sdk
    fi
    ;;
esac
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/build/blacklist-check" -w /build/blacklist-check \
    "$image" make PLATFORM="$platform" "$@"
