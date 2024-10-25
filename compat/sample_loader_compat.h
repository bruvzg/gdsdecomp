#pragma once
#include "compat/resource_loader_compat.h"
#include "core/io/resource.h"
#include "core/object/ref_counted.h"
#include "utility/resource_info.h"

class SampleConverterCompat : public ResourceCompatConverter {
	GDCLASS(SampleConverterCompat, ResourceCompatConverter);

public:
	virtual Ref<Resource> convert(const Ref<MissingResource> &res, ResourceInfo::LoadType p_type, int ver_major, Error *r_error = nullptr) override;
	virtual bool handles_type(const String &p_type, int ver_major) const override;
};
