extends Control

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

func rename_file(path:String):
	var new_path = path
	if path.get_file().ends_with(".tscn.converted.scn"):
		new_path = path.replace(".tscn.converted.scn",".tscn")
	elif path.get_file().ends_with(".tres.converted.res"):
		new_path = path.replace(".tres.converted.res",".tres")
	elif path.get_file().ends_with(".scn"):
		new_path = path.replace(".scn",".tscn")
	elif path.get_file().ends_with(".res"):
		new_path = path.replace(".res",".tres")
	return new_path


func _convert_bin_res_file(path: String, output_dir:String, existing_files: Array, converted_files: Array, failed_files: Array) -> int:
	ResourceLoader.set_abort_on_missing_resources(false)
	var new_path = rename_file(path)
	var deps = ResourceLoader.get_dependencies(output_dir.plus_file(path))
	for dep in deps:
		var dep_ext = dep.get_extension()
		if (dep_ext == "tscn" || dep_ext == "tres") and converted_files.find(dep) == -1:
			var bin_dep_path = ""
			if dep_ext == "tscn":
				if existing_files.find(dep.replace(".tscn",".tscn.converted.scn")) != -1:
					bin_dep_path = dep.replace(".tscn",".tscn.converted.scn")
				elif existing_files.find(dep.replace(".tscn",".scn")) != -1:
					bin_dep_path = dep.replace(".tscn",".scn")
			if dep_ext == "tres":
				if existing_files.find(dep.replace(".tres",".tres.converted.res")) != -1:
					bin_dep_path = dep.replace(".tres",".tres.converted.res")
				elif existing_files.find(dep.replace(".tres",".res")) != -1:
					bin_dep_path = dep.replace(".tres",".res")
			# can't find it
			if bin_dep_path == "":
				print("Error:" + path + " failed to find dependency for " + dep)
				failed_files.push_back(path)
				return ERR_CANT_ACQUIRE_RESOURCE

			if _convert_bin_res_file(bin_dep_path, output_dir, existing_files, converted_files, failed_files) != OK:
					print("Error:" + path + " failed to find dependency for " + dep)
					failed_files.push_back(path)
					return ERR_CANT_ACQUIRE_RESOURCE

	var ria = ResourceLoader.load_interactive(output_dir.plus_file(path))
	if (ria.wait() != OK):
		print("ERROR: "+ path +": Failed to load file")
		return ERR_CANT_ACQUIRE_RESOURCE

	if ResourceSaver.save(output_dir.plus_file(new_path), ria.get_resource()) != OK:
		print("ERROR: "+ path +": Failed to save converted file")
		failed_files.append(path)
		return ERR_CANT_ACQUIRE_RESOURCE
	converted_files.push_back(new_path)
	print("Converted file: " + path + " to " + new_path)
	return OK


func convert_bin_res_files(files: Array, root: String):
	var converted_files = []
	var failed_files = []
	for path in files:
		_convert_bin_res_file(path, root, files, converted_files, failed_files)
	# Try the failed files again
	var new_failed_files = []
	for path in failed_files:
		_convert_bin_res_file(path, root, files, converted_files, new_failed_files)
	
	#report:
	print("*****FAILED TO CONVERT*****")
	for path in new_failed_files:
		print("*** " + path)


func list_dir_rel(root: String, filter:String="", rel:String ="") -> Array:
	var list:Array = []
	var dir = Directory.new()
	var filters = filter.split(",")
	var subdirs = []
	if (dir.open(root.plus_file(rel)) == OK):
		dir.list_dir_begin()
		var filename = dir.get_next()
		while (filename != ""):
			if dir.current_is_dir() && !(filename == "." || filename == ".."):
				subdirs.push_back(filename)
			elif filter == "":
				list.push_back(rel.plus_file(filename))
			else:
				for fil in filters:
					if filename.get_extension() == fil:
						list.push_back(rel.plus_file(filename))
						break
			filename = dir.get_next()
	for subdir in subdirs:
		list.append_array(list_dir_rel(root, filter, rel.plus_file(subdir)))
	return list

func test_scnthing(output_dir:String):
	var list = list_dir_rel(output_dir, "scn,res")
	for f in list:
		print("*** " + f)
	convert_bin_res_files(list, output_dir)


func dump_files(exe_file:String, output_dir:String):
	var thing = PckDumper.new()
	if thing.load_pck(exe_file) == OK:
		print("Loaded this shit!")
		var version:String = thing.get_engine_version();
		print("Version: " + version)
		#thing.check_md5_all_files()
		if thing.pck_dump_to_dir(output_dir) != OK:
			print("error dumping to dir")
			return
		print("haldo")
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
						da.remove(f)
						if da.file_exists(f.replace(".gdc",".gd.remap")):
							da.remove(f.replace(".gdc",".gd.remap"))
						print("successfully decompiled " + f)
					else:
						print("error failed to save "+ f)
	else:
		print("ERROR: failed to load exe")

func handle_cli():
	var args = OS.get_cmdline_args()
	var exe_file:String = ""
	var output_dir: String = ""
	for i in range(args.size()):
		var arg:String = args[i]
		if arg == "--help":
			print("Usage: GDRE_Tools.exe --no-window --extract=<PAK_OR_EXE> --output-dir=<DIR>")
			get_tree().quit()
		if arg.begins_with("--extract"):
			exe_file = get_arg_value(arg)
		if arg.begins_with("--output-dir"):
			output_dir = get_arg_value(arg)
	if exe_file != "":
		if output_dir == "":
			print("Error: use --output-dir=<dir> when using --extract")
			get_tree().quit()
		else:
			dump_files(exe_file, output_dir)
	get_tree().quit()	



		
			
		
