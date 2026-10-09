#!/bin/bash
set -e

# Manually set compatibility version. Update when compatibility breaks
export COMPATIBILITY_VERSION=1

# Automatically set version based on git tags
export RELEASE_VERSION=$(git describe --tags --abbrev)
export APWORLD_VERSION=$(git describe --tags --no-abbrev | sed -Ee 's/(v)?([0-9]*\.[0-9]+\.[0-9]+)(-[0-9]+)?/\2/g')

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

envsubst < ReleaseDataTemplate.py > ./World/smo/Connector/ReleaseData.py

# Zip the world
jq -S ".world_version = \"${APWORLD_VERSION}\"" archipelago.json > ./World/smo/archipelago.json
cd ./World
rm -rv smo/__pycache__ 2> /dev/null || true
zip -r ../smo.apworld ./smo
rm smo/archipelago.json

# Return to root
cd ..
