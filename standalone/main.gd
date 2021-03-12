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

func dump_files(exe_file:String, output_dir:String):
	var thing = PckDumper.new()
	if thing.load_pck(exe_file) == OK:
		print("Loaded this shit!")
		for f in thing.get_loaded_files():
			print(f)
		#thing.check_md5_all_files()
		if thing.pck_dump_to_dir(output_dir) != OK:
			print("error dumping to dir")
	else:
		print("ERROR: failed to load exe")
	


func handle_cli():
	var args = OS.get_cmdline_args()
	var exe_file:String = ""
	var output_dir: String = ""
	for i in range(args.size()):
		var arg:String = args[i]
		if arg == "--fart":
			print("GO AWAY!!!!!!!!!!")
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



		
			
		
