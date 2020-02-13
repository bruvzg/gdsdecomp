
#!/bin/bash

set -ev

scons --version
clang --version

cd ..
git clone --depth 1 https://github.com/godotengine/godot godot -b 3.2

if [ ! -d $(pwd)/godot/modules/gdsdecomp ]; then
	ln -s $(pwd)/gdsdecomp $(pwd)/godot/modules/gdsdecomp
fi

cd godot

scons -j2 platform=x11 bits=64 tools=no disable_3d=yes target=release debug_symbols=no optimize=size no_editor_splash=yes module_hdr_enabled=no module_bmp_enabled=no module_bullet_enabled=no module_jpg_enabled=no module_squish_enabled=no module_csg_enabled=no module_stb_vorbis_enabled=yes module_cvtt_enabled=no module_mbedtls_enabled=no module_dds_enabled=no module_tga_enabled=no module_enet_enabled=no module_thekla_unwrap_enabled=no module_etc_enabled=no module_mobile_vr_enabled=no module_theora_enabled=no module_ogg_enabled=no module_upnp_enabled=no module_gdnative_enabled=no module_opensimplex_enabled=no module_opus_enabled=no module_vorbis_enabled=no module_pvr_enabled=no module_webm_enabled=no module_recast_enabled=no module_webp_enabled=no module_websocket_enabled=no module_gridmap_enabled=no module_xatlas_unwrap_enabled=no use_static_cpp=yes builtin_freetype=yes builtin_libpng=yes builtin_zlib=yes use_llvm=yes
