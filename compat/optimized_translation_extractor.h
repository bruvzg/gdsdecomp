#ifndef __OPTIMIZED_TRANSLATION_EXTRACTOR_H__
#define __OPTIMIZED_TRANSLATION_EXTRACTOR_H__

#include "core/string/optimized_translation.h"

class OptimizedTranslationExtractor : public OptimizedTranslation {
	GDCLASS(OptimizedTranslationExtractor, OptimizedTranslation);

	struct oteBucket {
		int size;
		uint32_t func;

		struct oteElem {
			uint32_t key;
			uint32_t str_offset;
			uint32_t comp_size;
			uint32_t uncomp_size;
		};

		oteElem elem[1];
	};

	_FORCE_INLINE_ uint32_t hash(uint32_t d, const char *p_str) const {
		if (d == 0) {
			d = 0x1000193;
		}
		while (*p_str) {
			d = (d * 0x1000193) ^ uint32_t(*p_str);
			p_str++;
		}

		return d;
	}

public:
	void get_message_value_list(List<StringName> *r_messages) const;
	OptimizedTranslationExtractor() {}
};

#endif // __OPTIMIZED_TRANSLATION_EXTRACTOR_H__