extends Control

var ver_major = 2;

func _ready():
	$menu_background/version_lbl.text = $re_editor_standalone.get_version()
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
		$root.quit()
	return split_args[1]

func list_dir_rel(root: String, filter:String="", rel:String ="") -> Array:
	var list:Array = []
	var dir = Directory.new()
	var filters = filter.split(",")
	var subdirs = []
	if (dir.open(root.plus_file(rel)) == OK):
		dir.list_dir_begin()
		var fname:String = dir.get_next()
		while (fname != ""):
			if dir.current_is_dir() && !(fname == "." || fname == ".."):
				subdirs.push_back(fname)
			elif filter == "":
				list.push_back(rel.plus_file(fname))
			else:
				for fil in filters:
					if fname.get_extension() == fil:
						list.push_back(rel.plus_file(fname))
						break
			fname = dir.get_next()
	for subdir in subdirs:
		list.append_array(list_dir_rel(root, filter, rel.plus_file(subdir)))
	return list

func export_imports(output_dir:String):
	var importer:ImportExporter = ImportExporter.new()
	importer.load_import_files(output_dir, ver_major)
	var arr = importer.get_import_files()
	var failed_files = []
	for ifo in arr:
		#the path to the imported file
		var path:String
		# check if there's a single path
		if ifo.has("path") && ifo.get("path"):
			path = ifo.get("path")
		# If there isn't one, that means we likely have two imported resources from one source
		# Just use the first "dest_file" in "dest_files"
		elif ifo.has("dest_files") && ifo.get("dest_files"):
			var paths:PackedStringArray = ifo.get("dest_files")
			path = paths[0]
		
		#the original source file that we will convert the imported file to
		var source_file:String = ifo.get("source_file")
		var ext:String = ifo.get("source_file").get_extension();
		if ext == "wav":
			importer.convert_sample_to_wav(output_dir, path, source_file)
		elif ext == "ogg":
			importer.convert_oggstr_to_ogg(output_dir, path, source_file)
		elif ext == "png" && ver_major == 3:
			importer.convert_v3stex_to_png(output_dir, path, source_file)
		elif ext == "png" && ver_major == 2:
			importer.convert_v2tex_to_png(output_dir, path, source_file)
		elif ext == "tscn" || ext == "escn":
			importer.convert_res_bin_2_txt(output_dir, path, source_file)
			

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

	
func dump_files(exe_file:String, output_dir:String):
	var thing = PckDumper.new()
	print(exe_file)
	if thing.load_pck(exe_file) == OK:
		print("Successfully loaded PCK!")
		ver_major = thing.get_engine_version().split(".")[0].to_int()
		var version:String = thing.get_engine_version()
		print("Version: " + version)
		#thing.check_md5_all_files()
		if thing.pck_dump_to_dir(output_dir) != OK:
			print("error dumping to dir")
			return
		
		var decomp;
		if version.begins_with("2.1"):
			print("Version 2.1.x detected")
			decomp = GDScriptDecomp_ed80f45.new()
		elif version.begins_with("3.2"):
			print("Version 3.2.x detected")
			decomp = GDScriptDecomp_5565f55.new()
		else:
			print("unknown version, no decomp")
			return
		
		for f in thing.get_loaded_files():
			var da:Directory = Directory.new()
			da.open(output_dir)
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
						#da.remove(f)
						if da.file_exists(f.replace(".gdc",".gd.remap")):
							da.remove(f.replace(".gdc",".gd.remap"))
						print("successfully decompiled " + f)
					else:
						print("error failed to save "+ f)
		decomp.free()
	else:
		print("ERROR: failed to load exe")

func normalize_path(path: String):
	return path.replace("\\","/")

func print_import_info(output_dir: String):
	print("stuff")
	var importer:ImportExporter = ImportExporter.new()
	importer.load_import_files(output_dir, ver_major)
	var arr = importer.get_import_files()
	for ifo in arr:
		print(ifo)

func handle_cli():
	var args = OS.get_cmdline_args()
	#var args = ["--no-window", "--verbose", "--path", ".\\modules\\gdsdecomp\\standalone", "--extract=C:\\workspace\\godot-decomps\\d\\PandemicHero_v10.pck", "--output-dir=C:\\workspace\\godot-decomps\\ph-decomp"]

	var exe_file:String = ""
	var output_dir: String = ""
	for i in range(args.size()):
		var arg:String = args[i]
		if arg == "--help":
			print("Usage: GDRE_Tools.exe --no-window --extract=<PAK_OR_EXE> --output-dir=<DIR>")
			get_tree().quit()
		if arg.begins_with("--extract"):
			exe_file = normalize_path(get_arg_value(arg))
		if arg.begins_with("--output-dir"):
			output_dir = normalize_path(get_arg_value(arg))
	if exe_file != "":
		if output_dir == "":
			print("Error: use --output-dir=<dir> when using --extract")
			get_tree().quit()
		else:
			#print_import_info(output_dir)
			dump_files(exe_file, output_dir)
			export_imports(output_dir)
	get_tree().quit()	



		
			
		
