#ifndef __AXML_PARSER_H__
#define __AXML_PARSER_H__
#include <core/string/ustring.h>
#include <core/templates/vector.h>

class AXMLParser {
	String version_name;
	int version_code;
	String package_name;

	String godot_editor_version = "";
	String godot_library_version = "";

	int screen_orientation;

	bool screen_support_small;
	bool screen_support_normal;
	bool screen_support_large;
	bool screen_support_xlarge;

	int xr_mode_index;
	int hand_tracking_index;
	int hand_tracking_frequency_index;

	bool backup_allowed;
	bool classify_as_game;
	bool retain_data_on_uninstall;
	bool exclude_from_recents;
	bool is_resizeable;
	bool has_read_write_storage_permission;

	String xr_hand_tracking_metadata_name;
	String xr_hand_tracking_metadata_value;
	String xr_hand_tracking_version_name;
	String xr_hand_tracking_version_value;

	Vector<String> perms;

public:
	String get_godot_library_version_string() { return godot_library_version; };
	Error parse_manifest(Vector<uint8_t> &p_manifest);
};
#endif // __AMXL_PARSER_H__