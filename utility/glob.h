/**
 * MIT License
 *
 * Copyright (c) 2019 Pranav, 2023 Nikita Lita
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#pragma once
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "modules/regex/regex.h"
class Glob : public Object {
	GDCLASS(Glob, Object);

protected:
	static void _bind_methods();

public:
	/// \param pathname string containing a path specification
	/// \return vector of paths that match the pathname
	///
	/// Pathnames can be absolute (/usr/src/Foo/Makefile) or relative (../../Tools/*/*.gif)
	/// Pathnames can contain shell-style wildcards
	/// Broken symlinks are included in the results (as in the shell)
	static Vector<String> glob(const String &pathname, bool hidden = false);

	/// \param pathnames string containing a path specification
	/// \return vector of paths that match the pathname
	///
	/// Globs recursively.
	/// The pattern “**” will match any files and zero or more directories, subdirectories and
	/// symbolic links to directories.
	static Vector<String> rglob(const String &pathname, bool hidden = false);

	/// Runs `glob` against each pathname in `pathnames` and accumulates the results
	static Vector<String> glob_list(const Vector<String> &pathnames, bool hidden = false);

	/// Runs `rglob` against each pathname in `pathnames` and accumulates the results
	static Vector<String> rglob_list(const Vector<String> &pathnames, bool hidden = false);

	/// Returns true if the input path matche the glob pattern
	static bool fnmatch(const String &name, const String &pattern);
}; // namespace glob
