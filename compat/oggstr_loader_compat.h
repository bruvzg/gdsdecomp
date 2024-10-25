
#include "compat/resource_loader_compat.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/object/ref_counted.h"
#include "core/templates/vector.h"
#include "modules/vorbis/audio_stream_ogg_vorbis.h"
#include "scene/resources/texture.h"

#ifndef OGGSTR_LOADER_COMPAT_H
#define OGGSTR_LOADER_COMPAT_H
class OggStreamConverterCompat : public ResourceCompatConverter {
	GDCLASS(OggStreamConverterCompat, ResourceCompatConverter);

public:
	virtual Ref<Resource> convert(const Ref<MissingResource> &res, ResourceInfo::LoadType p_type, int ver_major, Error *r_error = nullptr) override;
	virtual bool handles_type(const String &p_type, int ver_major) const override;
};

#endif //OGGSTR_LOADER_COMPAT_H
