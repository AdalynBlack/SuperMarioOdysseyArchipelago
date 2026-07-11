#!/usr/bin/fish

cd ./starlight_patch_100/

cp -r ./atmosphere/contents/0100000000010000/romfs/* ./SMOAP/romfs/
cp -r ./atmosphere/contents/0100000000010000/exefs/* ./SMOAP/exefs/
cp -r ./atmosphere/exefs_patches/StarlightBase/* ./SMOAP/exefs/

set VERSION $(git describe --tags --abbrev)

rm SMO_Archipelago_*_Emu.zip
zip -r SMO_Archipelago_{$VERSION}_Emu.zip ./SMOAP/
