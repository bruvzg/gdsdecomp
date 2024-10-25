#pragma once
#include "compat/resource_loader_compat.h"
#include "core/io/resource.h"
#include "core/object/ref_counted.h"
#include "scene/resources/audio_stream_wav.h"
#include "utility/resource_info.h"

class SampleLoaderCompat : public RefCounted {
	GDCLASS(SampleLoaderCompat, RefCounted);

protected:
	static void _bind_methods();

public:
	static Ref<AudioStreamWAV> convert_adpcm_to_16bit(const Ref<AudioStreamWAV> &p_sample);
	static Ref<AudioStreamWAV> convert_qoa_to_16bit(const Ref<AudioStreamWAV> &p_sample);
};

class SampleConverterCompat : public ResourceCompatConverter {
	GDCLASS(SampleConverterCompat, ResourceCompatConverter);

public:
	virtual Ref<Resource> convert(const Ref<MissingResource> &res, ResourceInfo::LoadType p_type, int ver_major, Error *r_error = nullptr) override;
	virtual bool handles_type(const String &p_type, int ver_major) const override;
};
