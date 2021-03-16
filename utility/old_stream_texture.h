
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/object/reference.h"
#include "core/io/image.h"
#include "core/templates/vector.h"
#ifndef STREAM_TEXTURE_V3_H
#define STREAM_TEXTURE_V3_H

class StreamTextureV3 : public Reference{
    GDCLASS(StreamTextureV3, Reference)
    Ref<Image> image;
    Error _load_data(const String &p_path);
protected:
    static void _bind_methods();
public:
    Ref<Image> get_image();
    Error load(const String &p_path);
};
#endif