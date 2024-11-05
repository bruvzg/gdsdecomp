#pragma once
#include "core/io/image.h"
#include "exporters/resource_exporter.h"

class TextureExporter : public ResourceExporter {
	GDCLASS(TextureExporter, ResourceExporter);
	Error _export_file(const String &out_path, const String &res_path, int ver_major = 0);
	Error _convert_tex(const String &p_path, const String &p_dst, bool lossy, String &image_format);
	Error _convert_atex(const String &p_path, const String &p_dst, bool lossy, String &image_format);
	Error _convert_bitmap(const String &p_path, const String &p_dst, bool lossy);
	static Ref<Image> load_image_from_bitmap(const String p_path, Error *r_err);

public:
	static Error save_image(const String &dest_path, const Ref<Image> &img, bool lossy);
	virtual Error export_file(const String &out_path, const String &res_path) override;
	virtual Ref<ExportReport> export_resource(const String &output_dir, Ref<ImportInfo> import_infos) override;
	virtual void get_handled_types(List<String> *out) const override;
	virtual void get_handled_importers(List<String> *out) const override;
};