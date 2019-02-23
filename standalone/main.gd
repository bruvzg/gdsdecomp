extends Control

func _on_re_editor_standalone_write_log_message(message):
	$log_window.text += message
