#!/bin/sh

SCRIPT_DIR=$(dirname "$0")
cd "$SCRIPT_DIR/../../.."
cp -R "misc/dist/macos_template.app" "bin/"
mkdir -p "bin/macos_template.app/Contents/MacOS"
lipo -create bin/godot.macos.template_release.x86_64 bin/godot.macos.template_release.arm64 -output bin/godot.macos.template_release.universal
if [ -f bin/godot.macos.template_debug.x86_64 ] && [ -f bin/godot.macos.template_debug.arm64 ]; then
    lipo -create bin/godot.macos.template_debug.x86_64 bin/godot.macos.template_debug.arm64 -output bin/godot.macos.template_debug.universal
fi
if [ -f bin/godot.macos.template_debug.universal ]; then
    cp "bin/godot.macos.template_debug.universal" "bin/macos_template.app/Contents/MacOS/godot_macos_debug.universal"
else
    cp "bin/godot.macos.template_release.universal" "bin/macos_template.app/Contents/MacOS/godot_macos_debug.universal"
fi
cp "bin/godot.macos.template_release.universal" "bin/macos_template.app/Contents/MacOS/godot_macos_release.universal"
cd bin/
zip -r9 "macos.zip" "macos_template.app/"
cd ..
