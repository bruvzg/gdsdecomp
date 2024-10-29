#ifndef PCK_DUMPER_H
#define PCK_DUMPER_H

#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"
#include "core/io/resource.h"
#include "core/io/resource_importer.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/templates/rb_map.h"

#include "editor/gdre_progress.h"

#include "packed_file_info.h"
class PckDumper : public RefCounted {
	GDCLASS(PckDumper, RefCounted)
	bool skip_malformed_paths = false;
	bool skip_failed_md5 = false;
	bool should_check_md5 = false;
	bool opt_multi_thread = true;
	std::atomic<bool> cancelled = false;
	std::atomic<int> last_completed = 0;
	std::atomic<int> skipped_cnt = 0;
	std::atomic<int> broken_cnt = 0;

	bool _pck_file_check_md5(Ref<PackedFileInfo> &file);
	void _do_md5_check(uint32_t i, Ref<PackedFileInfo> *tokens);

protected:
	static void _bind_methods();

public:
	Error check_md5_all_files();
	Error _check_md5_all_files(Vector<String> &broken_files, int &checked_files, EditorProgressGDDC *pr);

	Error _pck_dump_to_dir(const String &dir, const Vector<String> &files_to_extract, EditorProgressGDDC *pr, String &error_string);
	Error pck_dump_to_dir(const String &dir, const Vector<String> &files_to_extract);
	//Error pck_dump_to_dir(const String &dir, const Vector<String> &files_to_extract);
};

#endif // PCK_DUMPER_H
