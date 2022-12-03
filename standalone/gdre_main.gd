extends Control

var ver_major = 0
var ver_minor = 0
var main : GDRECLIMain

func load_system_font(font_name : String, bold : bool, italic: bool) -> int:
	var path : String = OS.get_system_font_path(font_name);
	var the = theme
	if (path.is_empty()):
		return ERR_FILE_CANT_OPEN
	# We use the default font, we wont check for its existence
	var font : Font = theme.get_font("","")
	var fallbacks : Array[Font] = font.get_fallbacks()
	var sysfont : SystemFont = SystemFont.new()
	sysfont.set_font_names([font_name])
	fallbacks.append(sysfont)
	font.set_fallbacks(fallbacks)
	return OK

func load_system_fonts(fonts : PackedStringArray):
	for font in fonts:
		if load_system_font(font, false, false) == OK:
			#load_system_font(font, true, false)
			pass
		else:
			print("ERROR: Cannot open fallback font " + font);
	pass

func set_system_fonts():
	OS.get_system_fonts()
	# godot has a japanese font, need chinese and korean
	var WindowsFallbackFonts = [ 
		"Microsoft JhengHei", # traditional
		"Microsoft YaHei", # simplified
		"Malgun Gothic" # korean
	]
	var MacOSFallbackFonts = [ 
		"Apple LiGothic Medium", # traditional
		"STSong", # simplified
		"Apple Gothic", # korean
	]
	# a number of fallback fonts here, we can't be sure what distro we're using
	var LinuxFallbackFonts = [ 
		"UMingTW",  # traditional
		"UMingHK",  # traditional
		"Source Han Sans TW",  # traditional
		"WenQuanYiMicroHei", # simplified
		"WenQuanYiZenHei", # simplified
		"DroidSans Fallback", # traditional and simplified
		"Nanum Gothic", # korean
		"Nanum Barun Gothic" # korean
	]
	var platform = OS.get_name().to_lower()
	if platform == "windows":
		load_system_fonts(WindowsFallbackFonts)
	elif platform == "linux":
		load_system_fonts(LinuxFallbackFonts)
	elif platform == "macos":
		load_system_fonts(MacOSFallbackFonts)

func _ready():
	$version_lbl.text = $re_editor_standalone.get_version()
	# test_functions()
	# get_tree().quit()
	if !handle_cli():
		# We're in GUI mode, set the system fonts
		set_system_fonts()


func test_text_to_bin(txt_to_bin: String, output_dir: String):
	var importer:ImportExporter = ImportExporter.new()
	var dst_file = txt_to_bin.get_file().replace(".tscn", ".scn").replace(".tres", ".res")
	importer.convert_res_txt_2_bin(output_dir, txt_to_bin, dst_file)
	importer.convert_res_bin_2_txt(output_dir, output_dir.path_join(dst_file), dst_file.replace(".scn", ".tscn").replace(".res", ".tres"))

func _on_re_editor_standalone_write_log_message(message):
	$log_window.text += message
	$log_window.scroll_to_line($log_window.get_line_count() - 1)

func _on_version_lbl_pressed():
	OS.shell_open("https://github.com/bruvzg/gdsdecomp")

func get_arg_value(arg):
	var split_args = arg.split("=")
	if split_args.size() < 2:
		print("Error: args have to be in the format of --key=value (with equals sign)")
		get_tree().quit()
	return split_args[1]

func normalize_path(path: String):
	return path.replace("\\","/")
	
# func print_import_info_from_pak(pak_file: String):
# 	var pckdump = PckDumper.new()
# 	pckdump.load_pck(pak_file)
# 	var importer:ImportExporter = ImportExporter.new()
# 	importer.load_import_files()
# 	var arr = importer.get_import_files()
# 	print("size is " + str(arr.size()))
# 	for ifo in arr:
# 		var s:String = ifo.get_source_file() + " is "
# 		if ifo.get_import_loss_type() == 0:
# 			print(s + "lossless")
# 		elif ifo.get_import_loss_type() == -1:
# 			print(s + "unknown")
# 		else:
# 			print(s + "lossy")
# 		print((ifo as ImportInfo).to_string())
# 	pckdump.clear_data()
# 	importer.reset()
	
# func print_import_info(output_dir: String):
# 	var importer:ImportExporter = ImportExporter.new()
# 	importer.load_import_files()
# 	var arr = importer.get_import_files()
# 	print("size is " + str(arr.size()))
# 	for ifo in arr:
# 		var s:String = ifo.get_source_file() + " is "
# 		if ifo.get_import_loss_type() == 0:
# 			print(s + "lossless")
# 		elif ifo.get_import_loss_type() == -1:
# 			print(s + "unknown")
# 		else:
# 			print(s + "lossy")
# 		print((ifo as ImportInfo).to_string())
# 	importer.reset()

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

func export_imports(output_dir:String):
	var importer:ImportExporter = ImportExporter.new()
	importer.export_imports(output_dir)
	importer.reset()
				
	
func dump_files(output_dir:String) -> int:
	var err:int = OK;
	var pckdump = PckDumper.new()
	if err == OK:
		err = pckdump.check_md5_all_files()
		if err != OK and err != ERR_SKIP:
			print("Error md5")
			return err
		err = pckdump.pck_dump_to_dir(output_dir)
		if err != OK:
			print("error dumping to dir")
			return err
	else:
		print("ERROR: failed to load exe")
	return err;

func print_usage():
	print("Godot Reverse Engineering Tools")
	print("")
	print("Without any CLI options, the tool will start in GUI mode")
	print("\nGeneral options:")
	print("  -h, --help: Display this help message")
	print("\nFull Project Recovery options:")
	print("Usage: GDRE_Tools.exe --headless --recover=<PCK_OR_EXE_OR_EXTRACTED_ASSETS_DIR> [options]")
	print("")
	print("--recover=<GAME_PCK/EXE/APK_OR_EXTRACTED_ASSETS_DIR>\t\tThe PCK, APK, EXE, or extracted project directory to perform full project recovery on")
	print("--extract=<GAME_PCK/EXE/APK_OR_EXTRACTED_ASSETS_DIR>\t\tThe PCK, APK, or EXE to extract")

	print("\nOptions:\n")
	print("--key=<KEY>\t\tThe Key to use if project is encrypted (hex string)")
	print("--output-dir=<DIR>\t\tOutput directory, defaults to <NAME_extracted>, or the project directory if one of specified")

func recovery(input_file:String, output_dir:String, enc_key:String, extract_only: bool):
	var da:DirAccess
	var is_dir:bool = false
	var err: int = OK
	input_file = main.get_cli_abs_path(input_file)
	print(input_file)
	if output_dir == "":
		output_dir = input_file.get_basename()
		if output_dir.get_extension():
			output_dir += "_recovery"
	else:
		output_dir = main.get_cli_abs_path(output_dir)

	da = DirAccess.open(input_file.get_base_dir())

	#directory
	if da.dir_exists(input_file):
		if !da.dir_exists(input_file.path_join(".import")):
			print("Error: " + input_file + " does not appear to be a project directory")
			return
		else:
			is_dir = true
	#PCK/APK
	elif not da.file_exists(input_file):
		print("Error: failed to locate " + input_file)
		return

	main.open_log(output_dir)
	if (enc_key != ""):
		err = main.set_key(enc_key)
		if (err != OK):
			print("Error: failed to set key!")
			return
	
	err = main.load_pack(input_file)
	if (err != OK):
		print("Error: failed to open " + input_file)
		return

	print("Successfully loaded PCK!") 
	ver_major = main.get_engine_version().split(".")[0].to_int()
	ver_minor = main.get_engine_version().split(".")[1].to_int()
	var version:String = main.get_engine_version()
	print("Version: " + version)

	if output_dir != input_file and not is_dir: 
		if (da.file_exists(output_dir)):
			print("Error: output dir appears to be a file, not extracting...")
			return
	if is_dir:
		if extract_only:
			print("Why did you open a folder to extract it??? What's wrong with you?!!?")
			return
		if main.copy_dir(input_file, output_dir) != OK:
			print("Error: failed to copy " + input_file + " to " + output_dir)
			return
	else:
		err = dump_files(output_dir)
		if (err != OK):
			print("Error: failed to extract PAK file, not exporting assets")
			return
	if (extract_only):
		return
	export_imports(output_dir)

func print_version():
	print("Godot RE Tools " + main.get_gdre_version())

func handle_cli() -> bool:
	var args = OS.get_cmdline_args()
	var input_extract_file:String = ""
	var input_file:String = ""
	var output_dir: String = ""
	var enc_key: String = ""
	var txt_to_bin: String = ""
	if (args.size() == 0 or (args.size() == 1 and args[0] == "res://gdre_main.tscn")):
		return false
	main = GDRECLIMain.new()
	for i in range(args.size()):
		var arg:String = args[i]
		if arg == "--help":
			print_version()
			print_usage()
			get_tree().quit()
		if arg.begins_with("--version"):
			print_version()
			get_tree().quit()
		if arg.begins_with("--extract"):
			input_extract_file = normalize_path(get_arg_value(arg))
		if arg.begins_with("--recover"):
			input_file = normalize_path(get_arg_value(arg))
		if arg.begins_with("--txt-to-bin"):
			txt_to_bin = normalize_path(get_arg_value(arg))	
		elif arg.begins_with("--output-dir"):
			output_dir = normalize_path(get_arg_value(arg))
		elif arg.begins_with("--key"):
			enc_key = get_arg_value(arg)
	if input_file != "":
		recovery(input_file, output_dir, enc_key, false)
		main.clear_data()
		main.close_log()
		get_tree().quit()
	elif input_extract_file != "":
		recovery(input_extract_file, output_dir, enc_key, true)
		main.clear_data()
		main.close_log()
		get_tree().quit()
	elif txt_to_bin != "":
		txt_to_bin = main.get_cli_abs_path(txt_to_bin)
		output_dir = main.get_cli_abs_path(output_dir)
		test_text_to_bin(txt_to_bin, output_dir)
		get_tree().quit()
	else:
		print("ERROR: invalid option")

		print_usage()
		get_tree().quit()
	return true
