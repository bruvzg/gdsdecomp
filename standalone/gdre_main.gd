extends Control

var ver_major = 0
var ver_minor = 0
var scripts_only = false
var config: ConfigFile = null
var last_error = ""
var CONFIG_PATH = "user://gdre_settings.cfg"

func test_text_to_bin(txt_to_bin: String, output_dir: String):
	var importer:ImportExporter = ImportExporter.new()
	var dst_file = txt_to_bin.get_file().replace(".tscn", ".scn").replace(".tres", ".res")
	importer.convert_res_txt_2_bin(output_dir, txt_to_bin, dst_file)
	importer.convert_res_bin_2_txt(output_dir, output_dir.path_join(dst_file), dst_file.replace(".scn", ".tscn").replace(".res", ".tres"))

func _on_re_editor_standalone_dropped_files(files: PackedStringArray):
	if files.size() == 0:
		return
	$re_editor_standalone.pck_select_request(files[0])

func _on_re_editor_standalone_write_log_message(message):
	$log_window.text += message
	$log_window.scroll_to_line($log_window.get_line_count() - 1)

func register_dropped_files():
	pass
	var window = get_viewport()
	var err = window.files_dropped.connect(_on_re_editor_standalone_dropped_files)
	if err != OK:
		print("Error: failed to connect window to files_dropped signal")
		print("Type: " + self.get_class())
		print("name: " + str(self.get_name()))


var repo_url = "https://github.com/bruvzg/gdsdecomp"
var latest_release_url = "https://github.com/bruvzg/gdsdecomp/releases/latest"

func _on_version_lbl_pressed():
	OS.shell_open(repo_url)

func is_dev_version()-> bool:
	var version = GDRESettings.get_gdre_version()
	if "-dev" in version:
		return true
	return false

func check_version() -> bool:
	# check the version
	var http = HTTPRequest.new()
	# add it to the tree so it doesn't get deleted
	add_child(http)
	http.request_completed.connect(_on_version_check_completed)
	http.request("https://api.github.com/repos/bruvzg/gdsdecomp/releases/latest")
	return true
	
func is_new_version(new_version: String):
	var curr_version = GDRESettings.get_gdre_version()
	if curr_version == new_version:
		return false
	var curr_semver = SemVer.parse_semver(curr_version)
	var new_semver = SemVer.parse_semver(new_version)
	if curr_semver == null or new_semver == null:
		return false
	if new_semver.gt(curr_semver):
		return true
	return false


func _on_version_check_completed(_result, response_code, _headers, body):
	if response_code != 200:
		print("Error: failed to check for latest version")
		return
	var json = JSON.parse_string(body.get_string_from_utf8())
	var checked_version = json["tag_name"].strip_edges()
	var draft = json["draft"]
	var prerelease = json["prerelease"]
	var curr_version = GDRESettings.get_gdre_version()
	
	if draft or prerelease or not is_new_version(checked_version):
		return
	
	var update_str = "Update available! Click here! " + curr_version
	repo_url = latest_release_url
	$version_lbl.text = update_str
	print("New version of GDRE available: " + checked_version)
	print("Get it here: " + repo_url)

func _make_new_config():
	config = ConfigFile.new()
	set_showed_disclaimer(false)
	save_config()

func _load_config():
	config = ConfigFile.new()
	if config.load(CONFIG_PATH) != OK:
		_make_new_config()
	return true

func should_show_disclaimer():
	var curr_version = GDRESettings.get_gdre_version()
	var last_showed = config.get_value("General", "last_showed_disclaimer", "<NONE>")
	if last_showed == "<NONE>":
		return true
	if last_showed == curr_version:
		return false
	var curr_semver = SemVer.parse_semver(curr_version)
	var last_semver = SemVer.parse_semver(last_showed)
	if curr_semver == null or last_semver == null:
		return true
	return curr_semver.major == last_semver.major and curr_semver.minor == last_semver.minor

func set_showed_disclaimer(setting: bool):
	var version = "<NONE>"
	if setting:
		version = GDRESettings.get_gdre_version()
	config.set_value("General", "last_showed_disclaimer", version)

func save_config():
	if config == null:
		return ERR_DOES_NOT_EXIST
	if GDRESettings.is_pack_loaded():
		return ERR_FILE_CANT_WRITE
	return config.save(CONFIG_PATH)

func handle_quit(save_cfg = true):
	if GDRESettings.is_pack_loaded():
		GDRESettings.unload_pack()
	if save_cfg:
		var ret = save_config()
		if ret != OK and ret != ERR_DOES_NOT_EXIST:
			print("Couldn't save config file!")


func _ready():
	$version_lbl.text = GDRESettings.get_gdre_version()
	# If CLI arguments were passed in, just quit
	var args = get_sanitized_args()
	if handle_cli(args):
		get_tree().quit()
	else:
		_load_config()
		var show_disclaimer = should_show_disclaimer()
		show_disclaimer = show_disclaimer and len(args) == 0
		if show_disclaimer:
			set_showed_disclaimer(true)
			save_config()
		register_dropped_files()
		check_version()
		if show_disclaimer:
			$re_editor_standalone.show_about_dialog()
		if len(args) > 0:
			var window = get_viewport()
			window.emit_signal("files_dropped", args)

func _notification(what: int) -> void:
	if what == NOTIFICATION_EXIT_TREE:
		handle_quit()
	elif what == NOTIFICATION_WM_ABOUT:
		$re_editor_standalone.show_about_dialog()
	
func get_arg_value(arg):
	var split_args = arg.split("=")
	if split_args.size() < 2:
		last_error = "Error: args have to be in the format of --key=value (with equals sign)"
		return ""
	arg = split_args[1]
	if arg.begins_with("\"") and arg.ends_with("\""):
		return arg.substr(1, arg.length() - 2)
	if arg.begins_with("'") and arg.ends_with("'"):
		return arg.substr(1, arg.length() - 2)
	return split_args[1]

func normalize_path(path: String):
	return path.replace("\\","/")

func test_decomp(fname):
	var decomp = GDScriptDecomp_ed80f45.new()
	var f = fname
	if f.get_extension() == "gdc":
		print("decompiling " + f)
		#
		#if decomp.decompile_byte_code(output_dir.path_join(f)) != OK: 
		if decomp.decompile_byte_code(f) != OK: 
			print("error decompiling " + f)
		else:
			var text = decomp.get_script_text()
			var gdfile:FileAccess = FileAccess.open(f.replace(".gdc",".gd"), FileAccess.WRITE)
			if gdfile == null:
				gdfile.store_string(text)
				gdfile.close()
				#da.remove(f)
				print("successfully decompiled " + f)
			else:
				print("error failed to save "+ f)

func export_imports(output_dir:String, files: PackedStringArray):
	var importer:ImportExporter = ImportExporter.new()
	importer.export_imports(output_dir, files)
	importer.reset()
				
	
func dump_files(output_dir:String, files: PackedStringArray, ignore_checksum_errors: bool = false) -> int:
	var err:int = OK;
	var pckdump = PckDumper.new()
	if err == OK:
		err = pckdump.check_md5_all_files()
		if err != OK:
			if (err != ERR_SKIP and not ignore_checksum_errors):
				print("MD5 checksum failed, not proceeding...")
				return err
			elif (ignore_checksum_errors):
				print("MD5 checksum failed, but --ignore_checksum_errors specified, proceeding anyway...")
		err = pckdump.pck_dump_to_dir(output_dir, files)
		if err != OK:
			print("error dumping to dir")
			return err
	else:
		print("ERROR: failed to load exe")
	return err;

var MAIN_COMMANDS = ["--recover", "--extract", "--compile", "--list-bytecode-versions"]
var MAIN_CMD_NOTES = """Main commands:
--recover=<GAME_PCK/EXE/APK/DIR>   Perform full project recovery on the specified PCK, APK, EXE, or extracted project directory.
--extract=<GAME_PCK/EXE/APK>       Extract the specified PCK, APK, or EXE.
--compile=<GD_FILE>                Compile GDScript files to bytecode (can be repeated and use globs, requires --bytecode)
--list-bytecode-versions           List all available bytecode versions
"""

var GLOB_NOTES = """Notes on Include/Exclude globs:
	- Recursive patterns can be specified with '**'
		- Example: 'res://**/*.gdc' matches 'res://main.gdc', 'res://scripts/script.gdc', etc.)
	- Globs should be rooted to 'res://' or 'user://'
		- Example: 'res://*.gdc' will match all .gdc files in the root of the project, but not any of the subdirectories.
	- If not rooted, globs will be rooted to 'res://'
		- Example: 'addons/plugin/main.gdc' is equivalent to 'res://addons/plugin/main.gdc'
	- As a special case, if the glob has a wildcard and does contain a directory, it will be assumed to be a recursive pattern.
		- Example: '*.gdc' would be equivalent to 'res://**/*.gdc'
	- Include/Exclude globs will only match files that are actually in the project PCK/dir, not any non-present resource source files.
		Example: 
			- A project contains the file "res://main.gdc". 'res://main.gd' is the source file of 'res://main.gdc',
			  but is not included in the project PCK.
			- Performing project recovery with the include glob 'res://main.gd' would not recover 'main.gd'.
			- Performing project recovery with the include glob 'res://main.gdc' would recover 'res://main.gd'
"""

var RECOVER_OPTS_NOTES = """Recover/Extract Options:

--key=<KEY>                 The Key to use if project is encrypted as a 64-character hex string,
                            e.g.: '000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F'
--output-dir=<DIR>          Output directory, defaults to <NAME_extracted>, or the project directory if one of specified
--scripts-only              Only extract/recover scripts
--include=<GLOB>            Include files matching the glob pattern (can be repeated)
--exclude=<GLOB>            Exclude files matching the glob pattern (can be repeated)
--ignore-checksum-errors    Ignore MD5 checksum errors when extracting/recovering
"""
var COMPILE_OPTS_NOTES = """Compile Options:

--bytecode=<COMMIT_OR_VERSION>          Either the commit hash of the bytecode revision (e.g. 'f3f05dc'),
                                           or the version of the engine (e.g. '4.3.0')
--output-dir=<DIR>                      Directory where compiled files will be output to. 
                                          - If not specified, compiled files will be output to the same location 
                                          (e.g. '<PROJ_DIR>/main.gd' -> '<PROJ_DIR>/main.gdc')
"""
func print_usage():
	print("Godot Reverse Engineering Tools")
	print("")
	print("Without any CLI options, the tool will start in GUI mode")
	print("\nGeneral options:")
	print("  -h, --help: Display this help message")
	print("\nFull Project Recovery options:")
	print("Usage: GDRE_Tools.exe --headless <main_command> [options]")
	print(MAIN_CMD_NOTES)
	print(RECOVER_OPTS_NOTES)
	print(GLOB_NOTES)
	print(COMPILE_OPTS_NOTES)


# TODO: remove this hack
var translation_only = false
var SCRIPTS_EXT = ["gd", "gdc", "gde"]
func copy_dir(src:String, dst:String) -> int:
	var da:DirAccess = DirAccess.open(src)
	if !da.dir_exists(src):
		print("Error: " + src + " does not appear to be a directory")
		return ERR_FILE_NOT_FOUND
	da.make_dir_recursive(dst)
	da.copy_dir(src, dst)
	return OK

func get_cli_abs_path(path:String) -> String:
	if path.is_absolute_path():
		return path
	var exec_path = GDRESettings.get_exec_dir()
	var abs_path = exec_path.path_join(path).simplify_path()
	return abs_path

func normalize_cludes(cludes: PackedStringArray, dir = "res://") -> PackedStringArray:
	var new_cludes: PackedStringArray = []
	if dir != dir.get_base_dir() and dir.ends_with("/"):
		dir = dir.substr(0, dir.length() - 1)
	for clude in cludes:
		clude = clude.replace("\\", "/")
		if not "**" in clude and "*" in clude and not "/" in clude:
			new_cludes.append("res://**/" + clude)
			# new_cludes.append("user://**/" + clude)
			continue
		if clude.begins_with("/") and dir == "res://":
			clude = clude.substr(1, clude.length() - 1)
		if not clude.is_absolute_path():
			clude = dir.path_join(clude)
		elif dir != "res://":
			clude = clude.replace("res:/", dir)
		elif clude.begins_with("/"):
			clude = dir + clude.substr(1, clude.length() - 1)
		new_cludes.append(clude.simplify_path())
	return new_cludes

func recovery(  input_file:String,
				output_dir:String,
				enc_key:String,
				extract_only: bool,
				ignore_checksum_errors: bool = false,
				excludes: PackedStringArray = [],
				includes: PackedStringArray = []):
	var da:DirAccess
	var is_dir:bool = false
	var err: int = OK
	var parent_dir = "res://"
	input_file = get_cli_abs_path(input_file)
	if output_dir == "":
		output_dir = input_file.get_basename()
		if output_dir.get_extension():
			output_dir += "_recovery"
	else:
		output_dir = get_cli_abs_path(output_dir)

	da = DirAccess.open(input_file.get_base_dir())

	# check if da works
	if da == null:
		print_usage()
		print("Error: failed to locate parent dir for " + input_file)
		return
	#directory
	if da.dir_exists(input_file):
		if input_file.get_extension().to_lower() == "app":
			is_dir = false
		elif !da.dir_exists(input_file.path_join(".import")) && !da.dir_exists(input_file.path_join(".godot")):
			print_usage()
			print("Error: " + input_file + " does not appear to be a project directory")
			return
		else:
			parent_dir = input_file
			is_dir = true
	#PCK/APK
	elif not da.file_exists(input_file):
		print_usage()
		print("Error: failed to locate " + input_file)
		return

	GDRESettings.open_log_file(output_dir)
	if (enc_key != ""):
		err = GDRESettings.set_encryption_key_string(enc_key)
		if (err != OK):
			print_usage()
			print("Error: failed to set key!")
			return
	
	err = GDRESettings.load_pack(input_file, extract_only)
	if (err != OK):
		print_usage()
		print("Error: failed to open " + input_file)
		return

	print("Successfully loaded PCK!") 
	ver_major = GDRESettings.get_ver_major()
	ver_minor = GDRESettings.get_ver_minor()
	var version:String = GDRESettings.get_version_string()
	print("Version: " + version)
	var files: PackedStringArray = []
	if translation_only and scripts_only:
		print("Error: cannot specify both --translation-only and --scripts-only")
		return
	elif (translation_only or scripts_only and (includes.size() > 0 or excludes.size() > 0)):
		print("Error: cannot specify both --translation-only or --scripts-only and --include or --exclude")
		return
	if (translation_only):
		var new_files:PackedStringArray = []
		# remove all the non ".translation" files
		for file in GDRESettings.get_file_list():
			if (file.get_extension().to_lower() == "translation"):
				new_files.append(file)
		files.append_array(new_files)
		print("Translation only mode, only extracting translation files")
	elif scripts_only:
		files = GDRESettings.get_file_list()
		var new_files:PackedStringArray = []
		# remove all the non ".gd" files
		for file in GDRESettings.get_file_list():
			if (file.get_extension().to_lower() in SCRIPTS_EXT):
				new_files.append(file)
		files.append_array(new_files)
		print("Scripts only mode, only extracting scripts")
	else:
		if includes.size() > 0:
			includes = normalize_cludes(includes, parent_dir)
			files = Glob.rglob_list(includes)
			if len(files) == 0:
				print("Error: no files found that match includes")
				print("Includes: " + str(includes))
				print(GLOB_NOTES)
				return
		else:
			files = GDRESettings.get_file_list()
		if excludes.size() > 0:
			excludes = normalize_cludes(excludes, parent_dir)
			var result = Glob.fnmatch_list(files, excludes)
			for file in result:
				files.remove_at(files.rfind(file))

		if (includes.size() > 0 or excludes.size() > 0) and files.size() == 0:
			print("Error: no files to extract after filtering")
			if len(includes) > 0:
				print("Includes: " + str(includes))
			if len(excludes) > 0:
				print("Excludes: " + str(excludes))
			print(GLOB_NOTES)
			return

	if output_dir != input_file and not is_dir: 
		if (da.file_exists(output_dir)):
			print("Error: output dir appears to be a file, not extracting...")
			return
	if is_dir:
		if extract_only:
			print("Why did you open a folder to extract it??? What's wrong with you?!!?")
			return
		if copy_dir(input_file, output_dir) != OK:
			print("Error: failed to copy " + input_file + " to " + output_dir)
			return
	else:
		err = dump_files(output_dir, files, ignore_checksum_errors)
		if (err != OK):
			print("Error: failed to extract PAK file, not exporting assets")
			return
	if (extract_only):
		return
	export_imports(output_dir, files)

func print_version():
	print("Godot RE Tools " + GDRESettings.get_gdre_version())

func close_log():
	var path = GDRESettings.get_log_file_path()
	if path == "":
		return
	GDRESettings.close_log_file()
	print("Log file written to: " + path)
	print("Please include this file when reporting issues!")

func ensure_dir_exists(dir: String):
	var da:DirAccess = DirAccess.open(GDRESettings.get_exec_dir())
	if !da.dir_exists(dir):
		da.make_dir_recursive(dir)

func compile(files: PackedStringArray, bytecode_version: String, output_dir: String):
	if bytecode_version == "":
		print("Error: --bytecode is required for --compile (use --list-bytecode-versions to see available versions)")
		print(COMPILE_OPTS_NOTES)
		return
	if output_dir == "":
		output_dir = get_cli_abs_path(".") # default to current directory
	var decomp: GDScriptDecomp = null
	if '.' in bytecode_version:
		decomp = GDScriptDecomp.create_decomp_for_version(bytecode_version)
	else:
		decomp = GDScriptDecomp.create_decomp_for_commit(bytecode_version.hex_to_int())
	if decomp == null:
		print("Error: failed to create decompiler for commit " + bytecode_version + "!\n(run --list-bytecode-versions to see available versions)")
		return
	print("Compiling to bytecode version %x (%s)" % [decomp.get_bytecode_rev(), decomp.get_engine_version()])

	var new_files = Glob.rglob_list(files)
	if new_files.size() == 0:
		print("Error: no files found to compile")
		return
	ensure_dir_exists(output_dir)
	for file in new_files:
		print("Compiling " + file)
		if file.get_extension() != "gd":
			print("Error: " + file + " is not a GDScript file")
			continue
		var f = FileAccess.open(file, FileAccess.READ)
		var code = f.get_as_text()
		var bytecode: PackedByteArray = decomp.compile_code_string(code)
		if bytecode.is_empty():
			print("Error: failed to compile " + file)
			print(decomp.get_error_message())
			continue
		var out_file = output_dir.path_join(file.get_file().replace(".gd", ".gdc"))
		var out_f = FileAccess.open(out_file, FileAccess.WRITE)
		out_f.store_buffer(bytecode)
		out_f.close()
		print("Compiled " + file + " to " + out_file)		
	print("Compilation complete")

func get_sanitized_args():
	var args = OS.get_cmdline_args()
	#var scene_path = get_tree().root.scene_file_path
	var scene_path = "res://gdre_main.tscn"
	if args[0] == scene_path:
		return args.slice(1)
	return args

func handle_cli(args: PackedStringArray) -> bool:
	var input_extract_file:String = ""
	var input_file: String = ""
	var output_dir: String = ""
	var enc_key: String = ""
	var txt_to_bin: String = ""
	var ignore_md5: bool = false
	var compile_files = PackedStringArray()
	var bytecode_version: String = ""
	var main_args_cnt = 0
	var compile_cnt = 0
	var excludes: PackedStringArray = []
	var includes: PackedStringArray = []
	if (args.size() == 0):
		return false
	var any_commands = false
	for i in range(args.size()):
		var arg:String = args[i]
		if arg.begins_with("--"):
			any_commands = true
			break
	if any_commands == false:
		if not GDRESettings.is_headless():
			# not cli mode, drag-and-drop
			return false
		print_usage()
		print("ERROR: no command specified")
		return true
	for i in range(args.size()):
		var arg:String = args[i]
		if arg == "--help":
			print_version()
			print_usage()
			return true
		elif arg.begins_with("--version"):
			print_version()
			return true
		elif arg.begins_with("--extract"):
			input_extract_file = get_arg_value(arg).simplify_path()
			main_args_cnt += 1
		elif arg.begins_with("--recover"):
			input_file = get_arg_value(arg).simplify_path()
			main_args_cnt += 1
		elif arg.begins_with("--txt-to-bin"):
			txt_to_bin = get_arg_value(arg).simplify_path()	
			main_args_cnt += 1
		elif arg.begins_with("--output-dir"):
			output_dir = get_arg_value(arg).simplify_path()
		elif arg.begins_with("--scripts-only"):
			scripts_only = true
		elif arg.begins_with("--key"):
			enc_key = get_arg_value(arg)
		elif arg.begins_with("--ignore-checksum-errors"):
			ignore_md5 = true
		elif arg.begins_with("--translation-only"):
			translation_only = true
		elif arg.begins_with("--list-bytecode-versions"):
			var versions = GDScriptDecomp.get_bytecode_versions()
			print("\n--- Available bytecode versions:")
			for version in versions:
				print(version)
			return true
		elif arg.begins_with("--bytecode"):
			bytecode_version = get_arg_value(arg)
		elif arg.begins_with("--compile"):
			if compile_files.size() == 0:
				main_args_cnt += 1
			compile_files.append(get_arg_value(arg))
		elif arg.begins_with("--exclude"):
			excludes.append(get_arg_value(arg))
		elif arg.begins_with("--include"):
			includes.append(get_arg_value(arg))
		else:
			print_usage()
			print("ERROR: invalid option '" + arg + "'")
			return true
		if last_error != "":
			print_usage()
			print(last_error)
			return true
	if main_args_cnt > 1:
		print_usage()
		print("ERROR: invalid option! Must specify only one of " + ", ".join(MAIN_COMMANDS))
		return true
	if compile_files.size() > 0:
		compile(compile_files, bytecode_version, output_dir)
	elif input_file != "":
		recovery(input_file, output_dir, enc_key, false, ignore_md5, excludes, includes)
		GDRESettings.unload_pack()
		close_log()
	elif input_extract_file != "":
		recovery(input_extract_file, output_dir, enc_key, true, ignore_md5, excludes, includes)
		GDRESettings.unload_pack()
		close_log()
	elif txt_to_bin != "":
		txt_to_bin = get_cli_abs_path(txt_to_bin)
		output_dir = get_cli_abs_path(output_dir)
		test_text_to_bin(txt_to_bin, output_dir)
	else:
		print_usage()
		print("ERROR: invalid option! Must specify one of " + ", ".join(MAIN_COMMANDS))
	return true
