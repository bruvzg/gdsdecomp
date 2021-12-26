extends Control

var ver_major = 0;
var ver_minor = 0;
func _ready():
	$menu_background/version_lbl.text = $re_editor_standalone.get_version()
	# test_functions()
	# get_tree().quit()

	handle_cli()

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

func export_imports(output_dir:String):
	var importer:ImportExporter = ImportExporter.new()
	importer.load_import_files(output_dir, ver_major, ver_minor)
	var arr = importer.get_import_files()
	var failed_files = []
	print("Number of import files: " + str(arr.size()))
	importer.export_imports(output_dir)
	importer.reset()
			

func test_decomp(fname):
	var decomp = GDScriptDecomp_ed80f45.new()
	var f = fname
	if f.get_extension() == "gdc":
		print("decompiling " + f)
		#
		#if decomp.decompile_byte_code(output_dir.plus_file(f)) != OK: 
		if decomp.decompile_byte_code(f) != OK: 
			print("error decompiling " + f)
		else:
			var text = decomp.get_script_text()
			var gdfile:File = File.new()
			if gdfile.open(f.replace(".gdc",".gd"), File.WRITE) == OK:
				gdfile.store_string(text)
				gdfile.close()
				#da.remove(f)
				print("successfully decompiled " + f)
			else:
				print("error failed to save "+ f)

	
func dump_files(exe_file:String, output_dir:String, enc_key:String = "") -> int:
	var err:int = OK;
	var pckdump = PckDumper.new()
	print(exe_file)
	if (enc_key != ""):
		pckdump.set_key(enc_key)
	err = pckdump.load_pck(exe_file)
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

		var decomp;
		# TODO: instead of doing this, run the detect bytecode script
		if version.begins_with("1.0"):
			decomp = GDScriptDecomp_e82dc40.new()
		elif version.begins_with("1.1"):
			decomp = GDScriptDecomp_65d48d6.new()
		elif version.begins_with("2.0"):
			decomp = GDScriptDecomp_23441ec.new()
		elif version.begins_with("2.1"):
			decomp = GDScriptDecomp_ed80f45.new()
		elif version.begins_with("3.0"):
			decomp = GDScriptDecomp_054a2ac.new()
		elif version.begins_with("3.1"):
			decomp = GDScriptDecomp_514a3fb.new()
		elif version.begins_with("3.2"):
			decomp = GDScriptDecomp_5565f55.new()
		# Did 3.3 add new GDScript features?
		elif version.begins_with("3.3"):
			decomp = GDScriptDecomp_5565f55.new()
		else:
			print("Unknown version, no decomp")
			pckdump.clear_data()
			return err

		print("Script version "+ version.substr(0,3)+".x detected")

		for f in pckdump.get_loaded_files():
			var da:Directory = Directory.new()
			da.open(output_dir)
			f = f.replace("res://", "")
			if f.get_extension() == "gdc":
				print("decompiling " + f)
				if decomp.decompile_byte_code(output_dir.plus_file(f)) != OK: 
					print("error decompiling " + f)
				else:
					var text = decomp.get_script_text()
					var gdfile:File = File.new()
					if gdfile.open(output_dir.plus_file(f.replace(".gdc",".gd")), File.WRITE) == OK:
						gdfile.store_string(text)
						gdfile.close()
						da.remove(f)
						if da.file_exists(f.replace(".gdc",".gd.remap")):
							da.remove(f.replace(".gdc",".gd.remap"))
						print("successfully decompiled " + f)
					else:
						print("error failed to save "+ f)
		decomp.free()
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
	importer.load_import_files("res://", 0)
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
	importer.load_import_files(output_dir, 0)
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
	print("\nExport to Project options:")
	print("Usage: GDRE_Tools.exe --no-window --extract=<PAK_OR_EXE> --output-dir=<DIR> [options]")
	print("")
	print("--extract=<PAK_OR_EXE>\t\tThe Pak or EXE to extract")
	print("--output-dir=<DIR>\t\tOutput directory")
	print("\nOptions:\n")
	print("--key=<KEY>\t\tThe Key to use if PAK/EXE is encrypted (hex string)")
	#print("View Godot assets, extract Godot PAK files, and export Godot projects")

func handle_cli():
	var args = OS.get_cmdline_args()

	var exe_file:String = ""
	var output_dir: String = ""
	var enc_key: String = ""
	for i in range(args.size()):
		var arg:String = args[i]
		if arg == "--help":
			print_usage()
			get_tree().quit()
		if arg.begins_with("--extract"):
			exe_file = normalize_path(get_arg_value(arg))
		elif arg.begins_with("--output-dir"):
			output_dir = normalize_path(get_arg_value(arg))
		elif arg.begins_with("--key"):
			enc_key = get_arg_value(arg)
	if exe_file != "":
		if output_dir == "":
			print("Error: use --output-dir=<dir> when using --extract")
			print("")
			print_usage()
		else:
			var main = GDRECLIMain.new()
			exe_file = main.get_cli_abs_path(exe_file)
			output_dir = main.get_cli_abs_path(output_dir)
			main.open_log(output_dir)
			#debugging
			#print_import_info(output_dir)
			#print_import_info_from_pak(exe_file)
			var err = dump_files(exe_file, output_dir, enc_key)
			if (err == OK):
				export_imports(output_dir)
			else:
				print("Error: failed to extract PAK file, not exporting assets")
			main.close_log()
		get_tree().quit()
