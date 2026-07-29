#!/usr/bin/fish

set -x ISEMU 1

# Remove files that change with ISEMU
rm ./build100/StageSceneStateServerConfig.* ./build100/Socket* ./build100/logger.*

./docker-build.sh; or return

cd ./starlight_patch_100/; or return

cp -r ./atmosphere/contents/0100000000010000/romfs/* ./SMOAP/romfs/; or return
cp -r ./atmosphere/contents/0100000000010000/exefs/* ./SMOAP/exefs/; or return
cp -r ./atmosphere/exefs_patches/StarlightBase/* ./SMOAP/exefs/; or return

set VERSION $(git describe --tags --abbrev)
echo $VERSION > ./SMOAP/SMOAP_VERSION

rm -f SMO_Archipelago_*_Emu.zip
zip -r SMO_Archipelago_{$VERSION}_Emu.zip ./SMOAP/
