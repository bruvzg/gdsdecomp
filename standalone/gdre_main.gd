extends Control

var ver_major = 0
var ver_minor = 0
var main : GDRECLIMain

func _ready():
	$menu_background/version_lbl.text = $re_editor_standalone.get_version()
	# test_functions()
	# get_tree().quit()

	handle_cli()

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

func export_imports(output_dir:String, enc_key:String):
	var importer:ImportExporter = ImportExporter.new()
	if enc_key:
		main.set_key(enc_key)
	
	importer.load_import_files(output_dir, ver_major, ver_minor)
	importer.decompile_scripts(output_dir)
	var arr = importer.get_import_files()
	print("Number of import files: " + str(arr.size()))
	importer.export_imports(output_dir)
	importer.reset()
			

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

	
func dump_files(input_file:String, output_dir:String, enc_key:String = "") -> int:
	var err:int = OK;
	var pckdump = PckDumper.new()
	print(input_file)
	if (enc_key != ""):
		pckdump.set_key(enc_key)
	err = pckdump.load_pck(input_file)
	if err == OK:
		print("Successfully loaded PCK!")
		ver_major = pckdump.get_engine_version().split(".")[0].to_int()
		ver_minor = pckdump.get_engine_version().split(".")[1].to_int()
		var version:String = pckdump.get_engine_version()
		print("Version: " + version)
		err = pckdump.check_md5_all_files()
		if err != OK:
			print("Error md5")
			pckdump.clear_data()
			return err
		err = pckdump.pck_dump_to_dir(output_dir)
		if err != OK:
			print("error dumping to dir")
			pckdump.clear_data()
			return err
	else:
		print("ERROR: failed to load exe")
	pckdump.clear_data()
	return err;

func normalize_path(path: String):
	return path.replace("\\","/")

func print_import_info_from_pak(pak_file: String):
	var pckdump = PckDumper.new()
	pckdump.load_pck(pak_file)
	var importer:ImportExporter = ImportExporter.new()
	importer.load_import_files("res://", 0, 0)
	var arr = importer.get_import_files()
	print("size is " + str(arr.size()))
	for ifo in arr:
		var s:String = ifo.get_source_file() + " is "
		if ifo.get_import_loss_type() == 0:
			print(s + "lossless")
		elif ifo.get_import_loss_type() == -1:
			print(s + "unknown")
		else:
			print(s + "lossy")
		print((ifo as ImportInfo).to_string())
	pckdump.clear_data()
	importer.reset()
	
func print_import_info(output_dir: String):
	var importer:ImportExporter = ImportExporter.new()
	importer.load_import_files(output_dir, 0, 0)
	var arr = importer.get_import_files()
	print("size is " + str(arr.size()))
	for ifo in arr:
		var s:String = ifo.get_source_file() + " is "
		if ifo.get_import_loss_type() == 0:
			print(s + "lossless")
		elif ifo.get_import_loss_type() == -1:
			print(s + "unknown")
		else:
			print(s + "lossy")
		print((ifo as ImportInfo).to_string())
	importer.reset()

func print_usage():
	print("Godot Reverse Engineering Tools")
	print("")
	print("Without any CLI options, the tool will start in GUI mode")
	print("\nGeneral options:")
	print("  -h, --help: Display this help message")
	print("\nFull Project Recovery options:")
	print("Usage: GDRE_Tools.exe --headless --recover=<PAK_OR_EXE_OR_EXTRACTED_ASSETS_DIR> [options]")
	print("")
	print("--recover=<PAK_OR_EXE_OR_EXTRACTED_ASSETS_DIR>\t\tThe Pak, EXE, or extracted project directory to perform full project recovery on")
	print("\nOptions:\n")
	print("--key=<KEY>\t\tThe Key to use if PAK/EXE is encrypted (hex string)")
	print("--output-dir=<DIR>\t\tOutput directory, defaults to <NAME_extracted>, or the project directory if one of specified")

func extract(input_file:String, output_dir:String, enc_key:String):
	var da:DirAccess
	input_file = main.get_cli_abs_path(input_file)
	print(input_file)
	if output_dir == "":
		output_dir = input_file.get_basename()
		if output_dir.get_extension():
			output_dir += "_recovery"
	else:
		output_dir = main.get_cli_abs_path(output_dir)
	da = DirAccess.open(input_file.get_base_dir())
	if da.file_exists(input_file):
		main.open_log(output_dir)
		var err = dump_files(input_file, output_dir, enc_key)
		if err:
			print("ERROR: FAILED TO EXTRACT")
	else:
		print("Error: failed to find PCK file")

func recovery(input_file:String, output_dir:String, enc_key:String):
	var da:DirAccess
	input_file = main.get_cli_abs_path(input_file)
	print(input_file)
	if output_dir == "":
		output_dir = input_file.get_basename()
		if output_dir.get_extension():
			output_dir += "_recovery"
	else:
		output_dir = main.get_cli_abs_path(output_dir)
	# debugging
	#print_import_info(output_dir)
	#print_import_info_from_pak(input_file)
	#actually an directory, just run export_imports
	da = DirAccess.open(input_file.get_base_dir())
	if da.dir_exists(input_file):
		
		if !da.dir_exists(input_file.path_join(".import")):
			print("Error: This does not appear to be a project directory")
		else:
			if output_dir != input_file:
				if main.copy_dir(input_file, output_dir) != OK:
					print("Error: failed to copy " + input_file + " to " + output_dir)
					return
			main.open_log(output_dir)
			export_imports(output_dir, enc_key)
	elif da.file_exists(input_file):
		main.open_log(output_dir)
		var err = dump_files(input_file, output_dir, enc_key)
		if (err == OK):
			export_imports(output_dir, enc_key)
		else:
			print("Error: failed to extract PAK file, not exporting assets")
	else:
		print("Error: failed to load " + input_file)
	


func handle_cli():
	var args = OS.get_cmdline_args()

	var input_extract_file:String = ""
	var input_file:String = ""
	var output_dir: String = ""
	var enc_key: String = ""
	var txt_to_bin: String = ""
	if (args.size() == 0):
		return
	for i in range(args.size()):
		var arg:String = args[i]
		if arg == "--help":
			print_usage()
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
		main = GDRECLIMain.new()
		recovery(input_file, output_dir, enc_key)
		main.close_log()
		get_tree().quit()
	elif input_extract_file != "":
		main = GDRECLIMain.new()
		extract(input_extract_file, output_dir, enc_key)
		main.close_log()
		get_tree().quit()
	elif txt_to_bin != "":
		main = GDRECLIMain.new()
		txt_to_bin = main.get_cli_abs_path(txt_to_bin)
		output_dir = main.get_cli_abs_path(output_dir)
		test_text_to_bin(txt_to_bin, output_dir)
		get_tree().quit()
	else:
		print("ERROR: invalid option")

		print_usage()
		get_tree().quit()
