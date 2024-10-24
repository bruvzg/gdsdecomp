#pragma once
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/object/ref_counted.h"
#include "core/templates/vector.h"
#include "modules/vorbis/audio_stream_ogg_vorbis.h"
#include "scene/resources/audio_stream_wav.h"
#include "scene/resources/texture.h"

class SampleLoaderCompat : public RefCounted {
	GDCLASS(SampleLoaderCompat, RefCounted);

protected:
	static void _bind_methods();

public:
	static Ref<AudioStreamWAV> load_wav(const String &p_path, Error *r_err);
	static Ref<AudioStreamWAV> convert_adpcm_to_16bit(const Ref<AudioStreamWAV> &p_sample);
	static Ref<AudioStreamWAV> convert_qoa_to_16bit(const Ref<AudioStreamWAV> &p_sample);
};
