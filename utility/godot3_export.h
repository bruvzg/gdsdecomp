
static const char *prop_renames[][2] = {
	{ "anchor/bottom", "anchor_bottom" }, // Control
	{ "anchor/left", "anchor_left" }, // Control
	{ "anchor/right", "anchor_right" }, // Control
	{ "anchor/top", "anchor_top" }, // Control
	{ "bbcode/bbcode", "bbcode_text" }, // RichTextLabel
	{ "bbcode/enabled", "bbcode_enabled" }, // RichTextLabel
	{ "bias/bias", "bias" }, // Joints2D
	{ "caret/block_caret", "caret_block_mode" }, // TextEdit
	{ "caret/caret_blink", "caret_blink" }, // LineEdit, TextEdit
	{ "caret/caret_blink_speed", "caret_blink_speed" }, // LineEdit, TextEdit
	{ "cell/center_x", "cell_center_x" }, // GridMap
	{ "cell/center_y", "cell_center_y" }, // GridMap
	{ "cell/center_z", "cell_center_z" }, // GridMap
	{ "cell/custom_transform", "cell_custom_transform" }, // TileMap
	{ "cell/half_offset", "cell_half_offset" }, // TileMap
	{ "cell/octant_size", "cell_octant_size" }, // GridMap
	{ "cell/quadrant_size", "cell_quadrant_size" }, // TileMap
	{ "cell/scale", "cell_scale" }, // GridMap
	{ "cell/size", "cell_size" }, // GridMap, TileMap
	{ "cell/tile_origin", "cell_tile_origin" }, // TileMap
	{ "cell/y_sort", "cell_y_sort" }, // TileMap
	{ "collision/bounce", "collision_bounce" }, // TileMap
	//{ "collision/exclude_nodes", "disable_collision" }, // Joint, Joint2D // Joint2D can be converted, not Joint, handle manually
	{ "collision/friction", "collision_friction" }, // TileMap
	{ "collision/layers", "collision_layer" }, // Area, Area2D, PhysicsBody, PhysicsBody2D, TileMap
	{ "collision/margin", "collision/safe_margin" }, // PhysicsBody, PhysicsBody2D
	{ "collision/mask", "collision_mask" }, // Area, Area2D, PhysicsBody, PhysicsBody2D, TileMap
	{ "collision/use_kinematic", "collision_use_kinematic" }, // TileMap
	{ "config/amount", "amount" }, // Particles2D
	{ "config/emitting", "emitting" }, // Particles2D
	{ "config/explosiveness", "explosiveness" }, // Particles2D
	{ "config/h_frames", "h_frames" }, // Particles2D
	{ "config/lifetime", "lifetime" }, // Particles2D
	{ "config/local_space", "local_coords" }, // Particles2D
	{ "config/preprocess", "preprocess" }, // Particles2D
	{ "config/texture", "texture" }, // Particles2D
	{ "config/time_scale", "speed_scale" }, // Particles2D
	{ "config/v_frames", "v_frames" }, // Particles2D
	{ "content_margin/bottom", "content_margin_bottom" }, // StyleBox
	{ "content_margin/left", "content_margin_left" }, // StyleBox
	{ "content_margin/right", "content_margin_right" }, // StyleBox
	{ "content_margin/top", "content_margin_top" }, // StyleBox
	{ "damping/compression", "damping_compression" }, // VehicleWheel
	{ "damping/relaxation", "damping_relaxation" }, // VehicleWheel
	{ "damp_override/angular", "angular_damp" }, // PhysicsBody, PhysicsBody2D
	{ "damp_override/linear", "linear_damp" }, // PhysicsBody, PhysicsBody2D
	{ "dialog/hide_on_ok", "dialog_hide_on_ok" }, // AcceptDialog
	{ "dialog/text", "dialog_text" }, // AcceptDialog
	{ "drag_margin/bottom", "drag_margin_bottom" }, // Camera2D
	{ "drag_margin/h_enabled", "drag_margin_h_enabled" }, // Camera2D
	{ "drag_margin/left", "drag_margin_left" }, // Camera2D
	{ "drag_margin/right", "drag_margin_right" }, // Camera2D
	{ "drag_margin/top", "drag_margin_top" }, // Camera2D
	{ "drag_margin/v_enabled", "drag_margin_v_enabled" }, // Camera2D
	{ "enabler/fixed_process_parent", "physics_process_parent" }, // VisibilityEnabler2D
	{ "enabler/freeze_bodies", "freeze_bodies" }, // VisibilityEnabler, VisibilityEnabler2D
	{ "enabler/pause_animated_sprites", "pause_animated_sprites" }, // VisibilityEnabler2D
	{ "enabler/pause_animations", "pause_animations" }, // VisibilityEnabler, VisibilityEnabler2D
	{ "enabler/pause_particles", "pause_particles" }, // VisibilityEnabler2D
	{ "enabler/process_parent", "process_parent" }, // VisibilityEnabler2D
	{ "expand_margin/bottom", "expand_margin_bottom" }, // StyleBox
	{ "expand_margin/left", "expand_margin_left" }, // StyleBox
	{ "expand_margin/right", "expand_margin_right" }, // StyleBox
	{ "expand_margin/top", "expand_margin_top" }, // StyleBox
	{ "extra_spacing/bottom", "extra_spacing_bottom" }, // DynamicFont
	{ "extra_spacing/char", "extra_spacing_char" }, // DynamicFont
	{ "extra_spacing/space", "extra_spacing_space" }, // DynamicFont
	{ "extra_spacing/top", "extra_spacing_top" }, // DynamicFont
	{ "flags/alpha_cut", "alpha_cut" }, // Sprite3D
	{ "flags/double_sided", "double_sided" }, // Sprite3D
	{ "flags/shaded", "shaded" }, // Sprite3D
	{ "flags/transparent", "transparent" }, // Sprite3D
	{ "focus_neighbour/bottom", "focus_neighbour_bottom" }, // Control
	{ "focus_neighbour/left", "focus_neighbour_left" }, // Control
	{ "focus_neighbour/right", "focus_neighbour_right" }, // Control
	{ "focus_neighbour/top", "focus_neighbour_top" }, // Control
	{ "font/font", "font_data" }, // DynamicFont
	{ "font/size", "size" }, // DynamicFont
	{ "font/use_filter", "use_filter" }, // DynamicFont
	{ "font/use_mipmaps", "use_mipmaps" }, // DynamicFont
	{ "geometry/cast_shadow", "cast_shadow" }, // GeometryInstance
	{ "geometry/extra_cull_margin", "extra_cull_margin" }, // GeometryInstance
	{ "geometry/material_override", "material_override" }, // GeometryInstance
	{ "geometry/use_baked_light", "use_in_baked_light" }, // GeometryInstance
	{ "hint/tooltip", "hint_tooltip" }, // Control
	{ "input/capture_on_drag", "input_capture_on_drag" }, // CollisionObject
	{ "input/pickable", "input_pickable" }, // CollisionObject2D
	{ "input/ray_pickable", "input_ray_pickable" }, // CollisionObject
	{ "invert/border", "invert_border" }, // Polygon2D
	{ "invert/enable", "invert_enable" }, // Polygon2D
	{ "is_pressed", "pressed" }, // BaseButton
	{ "limit/bottom", "limit_bottom" }, // Camera2D
	{ "limit/left", "limit_left" }, // Camera2D
	{ "limit/right", "limit_right" }, // Camera2D
	{ "limit/top", "limit_top" }, // Camera2D
	{ "margin/bottom", "margin_bottom" }, // Control, StyleBox
	{ "margin/left", "margin_left" }, // Control, StyleBox
	{ "margin/right", "margin_right" }, // Control, StyleBox
	{ "margin/top", "margin_top" }, // Control, StyleBox
	{ "material/material", "material" }, // CanvasItem
	{ "material/use_parent", "use_parent_material" }, // CanvasItem
	{ "mesh/mesh", "mesh" }, // MeshInstance
	{ "mesh/skeleton", "skeleton" }, // MeshInstance
	//{ "mode", "fill_mode" }, // TextureProgress & others // Would break TileMap and others, handle manually
	{ "motion/brake", "brake" }, // VehicleBody
	{ "motion/engine_force", "engine_force" }, // VehicleBody
	{ "motion/mirroring", "motion_mirroring" }, // ParallaxLayer
	{ "motion/offset", "motion_offset" }, // ParallaxLayer
	{ "motion/scale", "motion_scale" }, // ParallaxLayer
	{ "motion/steering", "steering" }, // VehicleBody
	{ "occluder/light_mask", "occluder_light_mask" }, // TileMap
	{ "params/attenuation/distance_exp", "attenuation_distance_exp" },
	{ "params/attenuation/max_distance", "attenuation_max_distance" },
	{ "params/attenuation/min_distance", "attenuation_min_distance" },
	{ "params/emission_cone/attenuation_db", "emission_cone_attenuation_db" },
	{ "params/emission_cone/degrees", "emission_cone_degrees" },
	{ "params/modulate", "self_modulate" },
	{ "params/pitch_scale", "pitch_scale" },
	{ "params/scale", "texture_scale" },
	{ "params/volume_db", "volume_db" },
	{ "patch_margin/bottom", "patch_margin_bottom" }, // Patch9Frame
	{ "patch_margin/left", "patch_margin_left" }, // Patch9Frame
	{ "patch_margin/right", "patch_margin_right" }, // Patch9Frame
	{ "patch_margin/top", "patch_margin_top" }, // Patch9Frame
	{ "percent/visible", "percent_visible" }, // ProgressBar
	{ "placeholder/alpha", "placeholder_alpha" }, // LineEdit
	{ "placeholder/text", "placeholder_text" }, // LineEdit
	//{ "playback/active", "playback_active" }, // AnimationPlayer, AnimationTreePlayer // properly renamed for AnimationPlayer, but not AnimationTreePlayer, handle manually
	{ "playback/default_blend_time", "playback_default_blend_time" }, // AnimationPlayer
	{ "playback/process_mode", "playback_process_mode" }, // AnimationPlayer, AnimationTreePlayer, Tween
	{ "playback/speed", "playback_speed" }, // AnimationPlayer, Tween
	{ "playback/repeat", "playback_speed" }, // AnimationPlayer
	{ "popup/exclusive", "popup_exclusive" }, // Popup
	{ "process/pause_mode", "pause_mode" }, // Node
	{ "radial_fill/center_offset", "radial_center_offset" }, // TextureProgress
	{ "radial_fill/fill_degrees", "radial_fill_degrees" }, // TextureProgress
	{ "radial_fill/initial_angle", "radial_initial_angle" }, // TextureProgress
	{ "range/exp_edit", "exp_edit" }, // Range
	{ "range/height", "range_height" }, // Light2D
	{ "range/item_mask", "range_item_cull_mask" }, // Light2D
	{ "range/layer_max", "range_layer_max" }, // Light2D
	{ "range/layer_min", "range_layer_min" }, // Light2D
	{ "range/max", "max_value" }, // Range
	{ "range/min", "min_value" }, // Range
	{ "range/page", "page" }, // Range
	{ "range/rounded", "rounded" }, // Range
	{ "range/step", "step" }, // Range
	{ "range/value", "value" }, // Range
	{ "range/z_max", "range_z_max" }, // Light2D
	{ "range/z_min", "range_z_min" }, // Light2D
	{ "rect/min_size", "rect_min_size" }, // Control
	{ "rect/pos", "rect_position" }, // Control
	{ "rect/rotation", "rect_rotation" }, // Control
	{ "rect/scale", "rect_scale" }, // Control
	{ "rect/size", "rect_size" }, // Control
	//{ "region", "region_enabled" }, // Sprite, Sprite3D // Not renamed for Texture, handle manually
	{ "resource/name", "resource_name" }, // Resource
	{ "resource/path", "resource_path" }, // Resource
	{ "root/root", "root_node" }, // AnimationPlayer
	{ "script/script", "script" }, // Object
	{ "scroll/base_offset", "scroll_base_offset" }, // ParallaxBackground
	{ "scroll/base_scale", "scroll_base_scale" }, // ParallaxBackground
	{ "scroll/horizontal", "scroll_horizontal_enabled" }, // ScrollContainer
	{ "scroll/ignore_camera_zoom", "scroll_ignore_camera_zoom" }, // ParallaxBackground
	{ "scroll/limit_begin", "scroll_limit_begin" }, // ParallaxBackground
	{ "scroll/limit_end", "scroll_limit_end" }, // ParallaxBackground
	{ "scroll/offset", "scroll_offset" }, // ParallaxBackground
	{ "scroll/vertical", "scroll_vertical_enabled" }, // ScrollContainer
	{ "shadow/buffer_size", "shadow_buffer_size" }, // Light2D
	{ "shadow/color", "shadow_color" }, // Light2D
	{ "shadow/enabled", "shadow_enabled" }, // Light2D
	{ "shadow/item_mask", "shadow_item_cull_mask" }, // Light2D
	{ "size_flags/horizontal", "size_flags_horizontal" }, // Control // Enum order got inverted Expand,Fill -> Fill,Expand, handle manually after rename
	{ "size_flags/stretch_ratio", "size_flags_stretch_ratio" }, // Control
	{ "size_flags/vertical", "size_flags_vertical" }, // Control // Enum order got inverted Expand,Fill -> Fill,Expand, handle manually after rename
	{ "smoothing/enable", "smoothing_enabled" }, // Camera2D
	{ "smoothing/speed", "smoothing_speed" }, // Camera2D
	{ "sort/enabled", "sort_enabled" }, // YSort
	{ "split/collapsed", "collapsed" }, // SplitContainer
	{ "split/dragger_visibility", "dragger_visibility" }, // SplitContainer
	{ "split/offset", "split_offset" }, // SplitContainer
	{ "stream/audio_track", "audio_track" }, // VideoPlayer
	{ "stream/autoplay", "autoplay" }, // VideoPlayer
	{ "stream/buffering_ms", "buffering_msec" }, // VideoPlayer
	{ "stream/loop", "loop" }, // Audio*
	{ "stream/loop_restart_time", "loop_offset" }, // Audio*
	{ "stream/paused", "paused" }, // VideoPlayer
	{ "stream/pitch_scale", "pitch_scale" }, // Audio*
	{ "stream/play", "playing" }, // Audio*
	{ "stream/stream", "stream" }, // VideoPlayer
	{ "stream/volume_db", "volume_db" }, // VideoPlayer
	{ "suspension/max_force", "suspension_max_force" }, // VehicleWheel
	{ "suspension/stiffness", "suspension_stiffness" }, // VehicleWheel
	{ "suspension/travel", "suspension_travel" }, // VehicleWheel
	{ "texture/offset", "texture_offset" }, // Polygon2D
	{ "texture/over", "texture_over" }, // TextureProgress
	{ "texture/progress", "texture_progress" }, // TextureProgress
	{ "texture/rotation", "texture_rotation_degrees" }, // Polygon2D
	{ "texture/scale", "texture_scale" }, // Polygon2D
	{ "textures/click_mask", "texture_click_mask" }, // TextureButton
	{ "textures/disabled", "texture_disabled" }, // TextureButton
	{ "textures/focused", "texture_focused" }, // TextureButton
	{ "textures/hover", "texture_hover" }, // TextureButton
	{ "textures/normal", "texture_normal" }, // TextureButton
	{ "textures/pressed", "texture_pressed" }, // TextureButton
	{ "texture/texture", "texture" }, // Polygon2D
	{ "texture/under", "texture_under" }, // TextureProgress
	{ "theme/theme", "theme" }, // Control
	{ "transform/local", "transform" }, // Spatial
	{ "transform/pos", "position" }, // Node2D
	{ "transform/rotation", "rotation_degrees" }, // Spatial
	{ "transform/rotation_rad", "rotation" }, // Spatial
	{ "transform/rot", "rotation_degrees" }, // Node2D
	{ "transform/scale", "scale" }, // Node2D, Spatial
	{ "transform/translation", "translation" }, // Spatial
	{ "type/steering", "use_as_steering" }, // VehicleWheel
	{ "type/traction", "use_as_traction" }, // VehicleWheel
	{ "vars/lifetime", "lifetime" }, // Particles
	{ "velocity/angular", "angular_velocity" }, // PhysicsBody, PhysicsBody2D
	{ "velocity/linear", "linear_velocity" }, // PhysicsBody, PhysicsBody2D
	{ "visibility", "visibility_aabb" }, // Particles
	{ "visibility/behind_parent", "show_behind_parent" }, // CanvasItem
	{ "visibility/light_mask", "light_mask" }, // CanvasItem
	{ "visibility/on_top", "show_on_top" }, // CanvasItem
	//{ "visibility/opacity", "modulate" }, // CanvasItem // Can't be converted this way, handle manually
	//{ "visibility/self_opacity", "self_modulate" }, // CanvasItem // Can't be converted this way, handle manually
	{ "visibility/visible", "visible" }, // CanvasItem, Spatial
	{ "wheel/friction_slip", "wheel_friction_slip" }, // VehicleWheel
	{ "wheel/radius", "wheel_radius" }, // VehicleWheel
	{ "wheel/rest_length", "wheel_rest_length" }, // VehicleWheel
	{ "wheel/roll_influence", "wheel_roll_influence" }, // VehicleWheel
	{ "window/title", "window_title" }, // Dialogs
	{ "z/relative", "z_as_relative" }, // Node2D
	{ "z/z", "z_index" }, // Node2D
	{ NULL, NULL }
};

static const char *type_renames[][2] = {
	{ "CanvasItemMaterial", "ShaderMaterial" },
	{ "CanvasItemShader", "Shader" },
	{ "ColorFrame", "ColorRect" },
	{ "ColorRamp", "Gradient" },
	{ "FixedMaterial", "SpatialMaterial" },
	{ "Patch9Frame", "NinePatchRect" },
	{ "ReferenceFrame", "ReferenceRect" },
	{ "SampleLibrary", "Resource" },
	{ "SamplePlayer2D", "AudioStreamPlayer2D" },
	{ "SamplePlayer", "Node" },
	{ "SoundPlayer2D", "Node2D" },
	{ "SpatialSamplePlayer", "AudioStreamPlayer3D" },
	{ "SpatialStreamPlayer", "AudioStreamPlayer3D" },
	{ "StreamPlayer", "AudioStreamPlayer" },
	{ "TestCube", "MeshInstance" },
	{ "TextureFrame", "TextureRect" },
	// Only for scripts
	{ "Matrix32", "Transform2D" },
	{ "Matrix3", "Basis" },
	{ "RawArray", "PoolByteArray" },
	{ "IntArray", "PoolIntArray" },
	{ "RealArray", "PoolRealArray" },
	{ "StringArray", "PoolStringArray" },
	{ "Vector2Array", "PoolVector2Array" },
	{ "Vector3Array", "PoolVector3Array" },
	{ "ColorArray", "PoolColorArray" },
	{ NULL, NULL }
};

static const char *signal_renames[][2] = {
	{ "area_enter", "area_entered" }, // Area, Area2D
	{ "area_enter_shape", "area_shape_entered" }, // Area, Area2D
	{ "area_exit", "area_exited" }, // Area, Area2D
	{ "area_exit_shape", "area_shape_exited" }, // Area, Area2D
	{ "body_enter", "body_entered" }, // Area, Area2D, PhysicsBody, PhysicsBody2D
	{ "body_enter_shape", "body_shape_entered" }, // Area, Area2D, PhysicsBody, PhysicsBody2D
	{ "body_exit", "body_exited" }, // Area, Area2D, PhysicsBody, PhysicsBody2D
	{ "body_exit_shape", "body_shape_exited" }, // Area, Area2D, PhysicsBody, PhysicsBody2D
	{ "enter_camera", "camera_entered" }, // VisibilityNotifier
	{ "enter_screen", "screen_entered" }, // VisibilityNotifier, VisibilityNotifier2D
	{ "enter_tree", "tree_entered" }, // Node
	{ "enter_viewport", "viewport_entered" }, // VisibilityNotifier2D
	{ "exit_camera", "camera_exited" }, // VisibilityNotifier
	{ "exit_screen", "screen_exited" }, // VisibilityNotifier, VisibilityNotifier2D
	{ "exit_tree", "tree_exited" }, // Node
	{ "exit_viewport", "viewport_exited" }, // VisibilityNotifier2D
	//{ "finished", "animation_finished" }, // AnimationPlayer, AnimatedSprite, but not StreamPlayer, handle manually
	{ "fixed_frame", "physics_frame" }, // SceneTree
	{ "focus_enter", "focus_entered" }, // Control
	{ "focus_exit", "focus_exited" }, // Control
	{ "input_event", "gui_input" }, // Control // FIXME: but not CollisionObject and CollisionObject2D, it should be handled manually
	{ "item_pressed", "id_pressed" }, // PopupMenu
	{ "modal_close", "modal_closed" }, // Control
	{ "mouse_enter", "mouse_entered" }, // CollisionObject, CollisionObject2D, Control
	{ "mouse_exit", "mouse_exited" }, // CollisionObject, CollisionObject2D, Control
	{ "tween_start", "tween_started" }, // Tween
	{ "tween_complete", "tween_completed" }, // Tween
	{ NULL, NULL }
};
