#ifndef GDRE_UTIL_FUNCTIONS_H
#define GDRE_UTIL_FUNCTIONS_H

#include "core/io/dir_access.h"

namespace gdreutil {
static Vector<String> get_recursive_dir_list(const String dir, const Vector<String> &wildcards = Vector<String>(), const bool absolute = true, const String rel = "", const bool &res = false) {
	Vector<String> ret;
	Error err;
	Ref<DirAccess> da = DirAccess::open(dir.path_join(rel), &err);
	ERR_FAIL_COND_V_MSG(da.is_null(), ret, "Failed to open directory " + dir);

	if (da.is_null()) {
		return ret;
	}
	String base = absolute ? dir : "";
	da->list_dir_begin();
	String f = da->get_next();
	while (!f.is_empty()) {
		if (f == "." || f == "..") {
			f = da->get_next();
			continue;
		} else if (da->current_is_dir()) {
			ret.append_array(get_recursive_dir_list(dir, wildcards, absolute, rel.path_join(f)));
		} else {
			if (wildcards.size() > 0) {
				for (int i = 0; i < wildcards.size(); i++) {
					if (f.get_file().match(wildcards[i])) {
						ret.append(base.path_join(rel).path_join(f));
						break;
					}
				}
			} else {
				ret.append(base.path_join(rel).path_join(f));
			}
		}
		f = da->get_next();
	}
	da->list_dir_end();
	return ret;
}

} //namespace gdreutil

#endif //GDRE_UTIL_FUNCTIONS_H
