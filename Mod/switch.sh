#!/usr/bin/fish

cd ./starlight_patch_100/

set VERSION $(git describe --tags --abbrev)

rm SMO_Archipelago_*_Switch.zip
zip -r SMO_Archipelago_{$VERSION}_Switch.zip ./atmosphere/
