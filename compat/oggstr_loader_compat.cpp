#include "oggstr_loader_compat.h"
#include "resource_loader_compat.h"
#include "utility/gdre_settings.h"

#include "core/io/file_access.h"

Vector<uint8_t> packet_data_to_byte_array(Array page_data) {
	Vector<uint8_t> data;
	for (int i = 0; i < page_data.size(); i++) {
		Array page_variant = page_data.get(i);
		for (int j = 0; j < page_variant.size(); j++) {
			const PackedByteArray &packet = page_variant.get(j);
			data.append_array(packet);
		}
	}
	return data;
}

Vector<uint8_t> OggStreamLoaderCompat::get_ogg_stream_data(const String &p_path, Error *r_err) const {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (r_err) {
		*r_err = err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, Vector<uint8_t>(), "Cannot open file '" + p_path + "'.");

	ResourceLoaderCompat *loader = memnew(ResourceLoaderCompat);
	loader->fake_load = true;
	loader->local_path = p_path;
	loader->res_path = p_path;
	err = loader->open_bin(f);
	if (r_err) {
		r_err = &err;
	}
	Vector<uint8_t> data;
	ERR_RFLBC_COND_V_MSG_CLEANUP(err != OK, Vector<uint8_t>(), "Cannot open resource '" + p_path + "'.", loader);
	if (loader->engine_ver_major == 4) {
		Ref<AudioStreamOggVorbis> sample = ResourceLoader::load(p_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
		if (r_err) {
			*r_err = err;
		}
		ERR_RFLBC_COND_V_MSG_CLEANUP(err != OK, Vector<uint8_t>(), "Cannot open resource '" + p_path + "'.", loader);
		memdelete(loader);
		return packet_data_to_byte_array(sample->get_packet_sequence()->get_packet_data());
	}
	err = loader->load();
	if (r_err) {
		r_err = &err;
	}

	ERR_RFLBC_COND_V_MSG_CLEANUP(err != OK, Vector<uint8_t>(), "Cannot load resource '" + p_path + "'.", loader);
	String name;
	// bool loop;
	// float loop_offset;
	List<ResourceProperty> lrp = loader->internal_index_cached_properties[loader->res_path];
	for (List<ResourceProperty>::Element *PE = lrp.front(); PE; PE = PE->next()) {
		ResourceProperty pe = PE->get();
		if (pe.name == "resource/name") {
			name = pe.value;
		} else if (pe.name == "data") {
			data = pe.value;
			// } else if (pe.name == "loop") {
			// 	loop = pe.value;
			// } else if (pe.name == "loop_offset") {
			// 	loop_offset = pe.value;
		}
	}
	memdelete(loader);
	return data;
}
void OggStreamLoaderCompat::_bind_methods() {}
