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

# Zip the world
cd ./World
rm -rv smo/__pycache__ 2> /dev/null || true
zip -r ../smo.apworld ./smo

# Return to root
cd ..
