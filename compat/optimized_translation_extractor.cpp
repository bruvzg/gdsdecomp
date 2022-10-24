#include "optimized_translation_extractor.h"
extern "C" {
#include "thirdparty/misc/smaz.h"
}
void OptimizedTranslationExtractor::get_message_value_list(List<StringName> *r_messages) const {
	Vector<int> hash_table;
	Vector<int> bucket_table;
	Vector<uint8_t> strings;
	Variant r_ret;
	ERR_FAIL_COND_MSG(!_get("hash_table", r_ret), "Fuck!");
	hash_table = r_ret;
	ERR_FAIL_COND_MSG(!_get("bucket_table", r_ret), "Fuck!");
	bucket_table = r_ret;
	ERR_FAIL_COND_MSG(!_get("strings", r_ret), "Fuck!");
	strings = r_ret;

	int htsize = hash_table.size();

	if (htsize == 0) {
		return;
	}

	const int *htr = hash_table.ptr();
	const uint32_t *htptr = (const uint32_t *)&htr[0];
	const int *btr = bucket_table.ptr();
	const uint32_t *btptr = (const uint32_t *)&btr[0];
	const uint8_t *sr = strings.ptr();
	const char *sptr = (const char *)&sr[0];

	for (int i = 0; i < htsize; i++) {
		uint32_t p = htptr[i];
		if (p == -1) {
			continue;
		}
		const oteBucket &bucket = *(const oteBucket *)&btptr[p];
		for (int j = 0; j < bucket.size; j++) {
			String rstr;
			if (bucket.elem[j].comp_size == bucket.elem[j].uncomp_size) {
				rstr.parse_utf8(&sptr[bucket.elem[j].str_offset], bucket.elem[j].uncomp_size);
			} else {
				CharString uncomp;
				uncomp.resize(bucket.elem[j].uncomp_size + 1);
				smaz_decompress(&sptr[bucket.elem[j].str_offset], bucket.elem[j].comp_size, uncomp.ptrw(), bucket.elem[j].uncomp_size);
				rstr.parse_utf8(uncomp.get_data());
			}
			r_messages->push_back(rstr);
		}
	}
}
