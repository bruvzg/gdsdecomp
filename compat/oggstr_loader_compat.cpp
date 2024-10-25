#include "oggstr_loader_compat.h"
#include "compat/resource_compat_binary.h"
#include "core/io/file_access.h"
#include "modules/vorbis/resource_importer_ogg_vorbis.h"
#include "ogg/ogg.h"
Error packet_sequence_to_raw_data(const Ref<OggPacketSequence> &packet_sequence, Vector<uint8_t> &r_data) {
	auto page_data = packet_sequence->get_packet_data();
	Vector<uint64_t> page_sizes;
	uint64_t total_estimated_size = 0;
	// page data is a vector of a vector of packedbytearrays
	// need to get all the sizes of the pages
	for (int i = 0; i < page_data.size(); i++) {
		uint64_t page_size = 0;
		// a page is an array of PackedByteArrays
		Array page = page_data[i];
		for (int j = 0; j < page.size(); j++) {
			int pkt_size = ((PackedByteArray)page[j]).size();
			page_size += pkt_size;
			// Header size is 26 + ceil(pkt_size / 255)
			total_estimated_size += pkt_size + (pkt_size / 255) + 100; //just fudge it
		}
		page_sizes.push_back(page_size);
	}
	uint64_t total_pagedata_body_size = [](const Vector<uint64_t> p_counts) {
		uint64_t total = 0;
		for (int i = 0; i < p_counts.size(); i++) {
			total += p_counts[i];
		}
		return total;
	}(page_sizes);
	uint64_t total_actual_body_size = 0;
	uint64_t total_acc_size = 0;
	auto playback = packet_sequence->instantiate_playback();
	ogg_packet *pkt;
	ogg_stream_state os_en;
	ogg_stream_init(&os_en, rand());

	int page_cursor = 0;
	bool reached_eos = false;
	r_data.resize_zeroed(total_estimated_size);
	while (playback->next_ogg_packet(&pkt) && !reached_eos) {
		page_cursor = playback->get_page_number();
		int page_size = page_sizes[page_cursor];
		if (pkt->e_o_s) {
			reached_eos = true;
		}
		ogg_stream_packetin(&os_en, pkt);
		ERR_FAIL_COND_V_MSG(ogg_stream_check(&os_en), ERR_FILE_CORRUPT, "Ogg stream is corrupt.");
		if (os_en.body_fill >= page_size || reached_eos) {
			if (os_en.body_fill < page_size) {
				WARN_PRINT("Reached EOS: Recorded page size is " + itos(page_size) + " but body fill is " + itos(os_en.body_fill) + ".");
			}
			ogg_page og;
			ERR_FAIL_COND_V_MSG(ogg_stream_flush_fill(&os_en, &og, page_size) == 0, ERR_FILE_CORRUPT, "Could not add page.");
			int cur_pos = total_acc_size;
			total_acc_size += og.header_len + og.body_len;
			total_actual_body_size += og.body_len;
			if (total_acc_size > r_data.size()) {
				WARN_PRINT("Resizing data to " + itos(total_acc_size));
				r_data.resize(total_acc_size);
			}
			memcpy(r_data.ptrw() + cur_pos, og.header, og.header_len);
			memcpy(r_data.ptrw() + cur_pos + og.header_len, og.body, og.body_len);
		}
	}
	page_cursor = playback->get_page_number();
	if (total_actual_body_size != total_pagedata_body_size) {
		WARN_PRINT("Actual body size" + itos(total_actual_body_size) + " does not equal the pagedata body size " + itos(total_pagedata_body_size) + ".");
	}
	// resize to the actual size
	r_data.resize(total_acc_size);

	ogg_stream_clear(&os_en);
	ERR_FAIL_COND_V_MSG(!reached_eos, ERR_FILE_CORRUPT, "All packets consumed before EOS.");
	ERR_FAIL_COND_V_MSG(page_cursor < page_sizes.size(), ERR_FILE_CORRUPT, "Did not write all pages before EOS.");
	ERR_FAIL_COND_V_MSG(r_data.size() < 4, ERR_FILE_CORRUPT, "Data is too small to be an Ogg stream.");
	ERR_FAIL_COND_V_MSG(r_data[0] != 'O' || r_data[1] != 'g' || r_data[2] != 'g' || r_data[3] != 'S', ERR_FILE_CORRUPT, "Header is missing in ogg data.");

	return OK;
}

Vector<uint8_t> OggStreamLoaderCompat::get_ogg_stream_data(const String &p_path, Error *r_err) const {
	Error _err = OK;
	if (!r_err) {
		r_err = &_err;
	}
	Vector<uint8_t> data;
	ResourceFormatLoaderCompatBinary rlcb;
	ResourceInfo i_info = rlcb.get_resource_info(p_path, r_err);
	ERR_FAIL_COND_V_MSG(*r_err != OK, Vector<uint8_t>(), "Cannot open resource '" + p_path + "'.");
	if (i_info.ver_major == 4) {
		Ref<AudioStreamOggVorbis> sample = ResourceCompatLoader::non_global_load(p_path, "", r_err);
		ERR_FAIL_COND_V_MSG(*r_err != OK, Vector<uint8_t>(), "Cannot open resource '" + p_path + "'.");
		auto packet_sequence = sample->get_packet_sequence();
		*r_err = packet_sequence_to_raw_data(packet_sequence, data);
		ERR_FAIL_COND_V_MSG(*r_err != OK, Vector<uint8_t>(), "Cannot convert packet sequence to raw data.");
	} else {
		auto res = rlcb.custom_load(p_path, ResourceInfo::LoadType::FAKE_LOAD, r_err);
		ERR_FAIL_COND_V_MSG(*r_err, Vector<uint8_t>(), "Cannot load resource '" + p_path + "'.");
		data = res->get("data");
	}
	return data;
}
void OggStreamLoaderCompat::_bind_methods() {}

// virtual Ref<Resource> convert(const Ref<MissingResource> &res, ResourceInfo::LoadType p_type, int ver_major, Error *r_error = nullptr) override;
// virtual bool handles_type(const String &p_type, int ver_major) const override;

Ref<Resource> OggStreamConverterCompat::convert(const Ref<MissingResource> &res, ResourceInfo::LoadType p_type, int ver_major, Error *r_error) {
	String name = ver_major < 3 ? res->get("resource/name") : res->get("resource_name");
	if (name.is_empty()) {
		name = res->get_name();
	}
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
	// else if (data_bytes != data.size()) {
}

bool OggStreamConverterCompat::handles_type(const String &p_type, int ver_major) const {
	return (p_type == "AudioStreamOGGVorbis" && ver_major <= 3);
}