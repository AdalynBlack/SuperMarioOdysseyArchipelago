#!/bin/bash
set -e

# Remove old release
rm ./SMO_Archipelago_*.zip
rm ./smo.apworld

# Build both mod releases
cd ./Mod
./emu.sh
./switch.sh

# Copy the mod releases for convenience
cd ..
cp ./Mod/starlight_patch_100/SMO_Archipelago_*.zip ./

# Zip the world
cd ./World
zip -r ../smo.apworld ./smo

# Return to root
cd ..
