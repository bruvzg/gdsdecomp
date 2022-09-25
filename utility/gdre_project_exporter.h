#ifndef GDRE_PROJECT_EXPORTER_H
#define GDRE_PROJECT_EXPORTER_H

#include "import_exporter.h"
#include "pck_dumper.h"

#include "core/object/ref_counted.h"

class GDREProjectExporter : public RefCounted {
	GDCLASS(GDREProjectExporter, RefCounted)

	Ref<PckDumper> pck_dumper;
	Ref<ImportExporter> import_exporter;

public:
	Error load_from_pak(const String &source, const String &key);

	Error export_to_project(const String &dest);
};

#endif