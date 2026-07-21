#!/usr/bin/fish

cd ./starlight_patch_100/

set VERSION $(git describe --tags --abbrev)
echo $VERSION > ./atmosphere/contents/0100000000010000/SMOAP_VERSION

rm -f SMO_Archipelago_*_Switch.zip
zip -r SMO_Archipelago_{$VERSION}_Switch.zip ./atmosphere/
