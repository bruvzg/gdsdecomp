#include <core/io/file_access.h>
#include <core/io/resource.h>
#include <core/io/dir_access.h>
#include <core/io/file_access.h>

namespace gdreutil{
    static Vector<String> get_recursive_dir_list(const String dir, const Vector<String> &wildcards = Vector<String>(), const bool absolute = true, const String rel = "", const bool &res = false) {
        Vector<String> ret;
        Error err;
        DirAccess *da = DirAccess::open(dir.plus_file(rel), &err);
        ERR_FAIL_COND_V_MSG(!da, ret, "Failed to open directory " + dir);

        if (!da) {
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
                ret.append_array(get_recursive_dir_list(dir, wildcards, absolute, rel.plus_file(f), res));
            } else {
                if (wildcards.size() > 0) {
                    for (int i = 0; i < wildcards.size(); i++) {
                        if (f.get_file().match(wildcards[i])) {
                            ret.append((res ? "res://" : "") + base.plus_file(rel).plus_file(f));
                            break;
                        }
                    }
                } else {
                    ret.append((res ? "res://" : "") + base.plus_file(rel).plus_file(f));
                }
            }
            f = da->get_next();
        }
        da->list_dir_end();
        memdelete(da);
        return ret;
    }
}