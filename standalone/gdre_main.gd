extends Control

var ver_major = 0
var ver_minor = 0
var scripts_only = false

func test_text_to_bin(txt_to_bin: String, output_dir: String):
	var importer:ImportExporter = ImportExporter.new()
	var dst_file = txt_to_bin.get_file().replace(".tscn", ".scn").replace(".tres", ".res")
	importer.convert_res_txt_2_bin(output_dir, txt_to_bin, dst_file)
	importer.convert_res_bin_2_txt(output_dir, output_dir.path_join(dst_file), dst_file.replace(".scn", ".tscn").replace(".res", ".tres"))

func _on_re_editor_standalone_write_log_message(message):
	$log_window.text += message
	$log_window.scroll_to_line($log_window.get_line_count() - 1)

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

func _ready():
	$version_lbl.text = $re_editor_standalone.get_version()
	# If CLI arguments were passed in, just quit
	if handle_cli():
		get_tree().quit()
	else:
		check_version()
	
func get_arg_value(arg):
	var split_args = arg.split("=")
	if split_args.size() < 2:
		print("Error: args have to be in the format of --key=value (with equals sign)")
		get_tree().quit()
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

func print_usage():
	print("Godot Reverse Engineering Tools")
	print("")
	print("Without any CLI options, the tool will start in GUI mode")
	print("\nGeneral options:")
	print("  -h, --help: Display this help message")
	print("\nFull Project Recovery options:")
	print("Usage: GDRE_Tools.exe --headless <main_command> [options]")
	print("\nMain commands:")
	print("--recover=<GAME_PCK/EXE/APK_OR_EXTRACTED_ASSETS_DIR>\t\tThe PCK, APK, EXE, or extracted project directory to perform full project recovery on")
	print("--extract=<GAME_PCK/EXE/APK_OR_EXTRACTED_ASSETS_DIR>\t\tThe PCK, APK, or EXE to extract")
	print("--compile=<GD_FILE>\t\tCompile GDScript files to bytecode (can be repeated and use globs, requires --bytecode)")
	print("--list-bytecode-versions\t\tList all available bytecode versions")

	print("\nOptions:\n")
	print("--key=<KEY>\t\tThe Key to use if project is encrypted (hex string)")
	print("--output-dir=<DIR>\t\tOutput directory, defaults to <NAME_extracted>, or the project directory if one of specified")
	print("--ignore-checksum-errors\t\tIgnore MD5 checksum errors when extracting/recovering")
	print("--translation-only\t\tOnly extract/recover translation files")
	print("--scripts-only\t\tOnly extract/recover scripts")
	print("--bytecode=<COMMIT_OR_VERSION>\t\tEither the commit hash of the bytecode revision (e.g. 'f3f05dc') or the version of the engine (e.g. '4.3.0')")


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

func recovery(  input_file:String,
				output_dir:String,
				enc_key:String,
				extract_only: bool,
				ignore_checksum_errors: bool = false):
	var da:DirAccess
	var is_dir:bool = false
	var err: int = OK
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
		print("Error: failed to locate parent dir for " + input_file)
		return
	#directory
	if da.dir_exists(input_file):
		if input_file.get_extension().to_lower() == "app":
			is_dir = false
		elif !da.dir_exists(input_file.path_join(".import")) && !da.dir_exists(input_file.path_join(".godot")):
			print("Error: " + input_file + " does not appear to be a project directory")
			return
		else:
			is_dir = true
	#PCK/APK
	elif not da.file_exists(input_file):
		print("Error: failed to locate " + input_file)
		return

	GDRESettings.open_log_file(output_dir)
	if (enc_key != ""):
		err = GDRESettings.set_encryption_key_string(enc_key)
		if (err != OK):
			print("Error: failed to set key!")
			return
	
	err = GDRESettings.load_pack(input_file, extract_only)
	if (err != OK):
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
	if (translation_only):
		var new_files:PackedStringArray = []
		# remove all the non ".translation" files
		for file in GDRESettings.get_file_list():
			if (file.get_extension().to_lower() == "translation"):
				new_files.append(file)
		files.append_array(new_files)
		print("Translation only mode, only extracting translation files")
	if scripts_only:
		files = GDRESettings.get_file_list()
		var new_files:PackedStringArray = []
		# remove all the non ".gd" files
		for file in GDRESettings.get_file_list():
			if (file.get_extension().to_lower() in SCRIPTS_EXT):
				new_files.append(file)
		files.append_array(new_files)
		print("Scripts only mode, only extracting scripts")

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
		print("Error: --bytecode is required for --compile")
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

func handle_cli() -> bool:
	var args = OS.get_cmdline_args()
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
	if (args.size() == 0 or (args.size() == 1 and args[0] == "res://gdre_main.tscn")):
		return false
	for i in range(args.size()):
		var arg:String = args[i]
		if arg == "--help":
			print_version()
			print_usage()
			get_tree().quit()
		elif arg.begins_with("--version"):
			print_version()
			get_tree().quit()
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
		else:
			print("ERROR: invalid option '" + arg + "'")
			print_usage()
			return true
	if main_args_cnt > 1:
		print("ERROR: invalid option! Must specify only one of " + ", ".join(MAIN_COMMANDS))
		print_usage()
		return true
	if compile_files.size() > 0:
		compile(compile_files, bytecode_version, output_dir)
	elif input_file != "":
		recovery(input_file, output_dir, enc_key, false, ignore_md5)
		GDRESettings.unload_pack()
		close_log()
	elif input_extract_file != "":
		recovery(input_extract_file, output_dir, enc_key, true, ignore_md5)
		GDRESettings.unload_pack()
		close_log()
	elif txt_to_bin != "":
		txt_to_bin = get_cli_abs_path(txt_to_bin)
		output_dir = get_cli_abs_path(output_dir)
		test_text_to_bin(txt_to_bin, output_dir)
	else:
		print("ERROR: invalid option! Must specify one of " + ", ".join(MAIN_COMMANDS))
		print_usage()
	return true
