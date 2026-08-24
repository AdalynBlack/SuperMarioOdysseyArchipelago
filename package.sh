#!/bin/bash
set -e

# Remove old release
rm -v ./SMO_Archipelago_*.zip 2> /dev/null || true
rm -v ./smo.apworld 2> /dev/null || true

# Build both mod releases
cd ./Mod
./emu.sh
./switch.sh

# Copy the mod releases for convenience
cd ..
cp ./Mod/starlight_patch_100/SMO_Archipelago_*.zip ./

VERSION=$(git describe --tags --no-abbrev | sed -Ee 's/(v)?([0-9]*\.[0-9]+\.[0-9]+)(-[0-9]+)?/\2/g')

# Zip the world
jq -S ".world_version = \"${VERSION:1}\"" archipelago.json > ./World/smo/archipelago.json
cd ./World
rm -rv smo/__pycache__ 2> /dev/null || true
zip -r ../smo.apworld ./smo
rm smo/archipelago.json

# Return to root
cd ..
