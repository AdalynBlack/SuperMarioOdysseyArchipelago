#!/usr/bin/fish

set -x ISEMU 0

# Remove files that change with ISEMU
rm ./build100/StageSceneStateServerConfig.* ./build100/Socket* ./build100/logger.*

./docker-build.sh; or return

cd ./starlight_patch_100/; or return

set VERSION $(git describe --tags --abbrev)
echo $VERSION > ./atmosphere/contents/0100000000010000/SMOAP_VERSION

rm -f SMO_Archipelago_*_Switch.zip
zip -r SMO_Archipelago_{$VERSION}_Switch.zip ./atmosphere/
