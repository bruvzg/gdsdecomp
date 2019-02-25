extends Control

func _on_re_editor_standalone_write_log_message(message):
	$log_window.text += message
	$log_window.scroll_to_line($log_window.get_line_count() - 1)

func _on_version_lbl_pressed():
	OS.shell_open("https://github.com/bruvzg/gdsdecomp")
