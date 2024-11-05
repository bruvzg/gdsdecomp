#include "oggstr_loader_compat.h"
#include "modules/vorbis/resource_importer_ogg_vorbis.h"

Ref<Resource> OggStreamConverterCompat::convert(const Ref<MissingResource> &res, ResourceInfo::LoadType p_type, int ver_major, Error *r_error) {
	String name = get_resource_name(res, ver_major);
	Vector<uint8_t> data = res->get("data");
	bool loop = res->get("loop");
	double loop_offset = res->get("loop_offset");
	Ref<AudioStreamOggVorbis> sample = ResourceImporterOggVorbis::load_from_buffer(data);
	if (!name.is_empty()) {
		sample->set_name(name);
	}
	sample->set_loop(loop);
	sample->set_loop_offset(loop_offset);
	return sample;
}

bool OggStreamConverterCompat::handles_type(const String &p_type, int ver_major) const {
	return (p_type == "AudioStreamOGGVorbis" && ver_major <= 3);
}