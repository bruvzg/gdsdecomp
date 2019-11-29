extends MainLoop

func _iteration(delta):
	return true

func _probe_func(fn):
	var scr = load("has_" + fn + ".gd")
	var x = scr.new()
	var s = x.test()
	return scr.can_instance() and s == 100

func _initialize():
	if _probe_func("ord"):
		OS.alert("3.2.0 release (5565f55 / 2019-08-26 / Bytecode version: 13) or newer")
		return
	if _probe_func("lerp_angle"):
		OS.alert("3.2 dev (6694c11 / 2019-07-20 / Bytecode version: 13)")
		return
	if _probe_func("posmod"):
		OS.alert("3.2 dev (a60f242 / 2019-07-19 / Bytecode version: 13)")
		return
	if _probe_func("move_toward"):
		OS.alert("3.2 dev (c00427a / 2019-06-01 / Bytecode version: 13)")
		return
	if _probe_func("step_decimals"):
		OS.alert("3.2 dev (620ec47 / 2019-05-01 / Bytecode version: 13)")
		return
	if _probe_func("is_equal_approx"):
		OS.alert("3.2 dev (7f7d97f / 2019-04-29 / Bytecode version: 13)")
		return
	if _probe_func("smoothstep"):
		OS.alert("3.1.1 release (514a3fb / 2019-03-19 / Bytecode version: 13)")
		return
	if not _probe_func("do") and _probe_func("push_error"):
		OS.alert("3.1.0 release (1a36141 / 2019-02-20 / Bytecode version: 13)")
		return
	if _probe_func("push_error"):
		OS.alert("3.1 beta 1 - beta 5 (1ca61a3 / 2018-10-31 / Bytecode version: 13)")
		return
	if _probe_func("puppetsync"):
		OS.alert("3.1 dev (d6b31da / 2018-09-15 / Bytecode version: 13)")
		return
	if _probe_func("typed"):
		OS.alert("3.1 dev (8aab9a0 / 2018-07-20 / Bytecode version: 13)")
		return
	if _probe_func("classname"):
		OS.alert("3.1 dev (a3f1ee5 / 2018-07-15 / Bytecode version: 13)")
		return
	if _probe_func("slavesync"):
		OS.alert("3.1 dev (8e35d93 / 2018-05-29 / Bytecode version: 12)")
		return
	if _probe_func("OS.alert_debug"):
		OS.alert("3.1 dev (3ea6d9f / 2018-05-28 / Bytecode version: 12)")
		return
	if _probe_func("get_stack"):
		OS.alert("3.1 dev (a56d6ff / 2018-05-17 / Bytecode version: 12)")
		return
	if _probe_func("is_instance_valid"):
		OS.alert("3.1 dev (ff1e7cf / 2018-05-07 / Bytecode version: 12)")
		return
	if _probe_func("cartesian2polar"):
		OS.alert("3.0.0 - 3.0.6 release (054a2ac / 2017-11-20 / Bytecode version: 12)")
		return
	if _probe_func("tau"):
		OS.alert("3.0 dev (91ca725 / 2017-11-12 / Bytecode version: 12)")
		return
	if _probe_func("wrapi"):
		OS.alert("3.0 dev (216a8aa / 2017-10-13 / Bytecode version: 12)")
		return
	if _probe_func("inverse_lerp"):
		OS.alert("3.0 dev (d28da86 / 2017-08-18 / Bytecode version: 12)")
		return
	if _probe_func("len"):
		OS.alert("3.0 dev (c6120e7 / 2017-08-07 / Bytecode version: 12)")
		return
	if _probe_func("is"):
		OS.alert("3.0 dev (015d36d / 2017-05-27 / Bytecode version: 12)")
		return
	if _probe_func("inf"):
		OS.alert("3.0 dev (5e938f0 / 2017-02-28 / Bytecode version: 12)")
		return
	if _probe_func("wc"):
		OS.alert("3.0 dev (c24c739 / 2017-01-20 / Bytecode version: 12)")
		return
	if _probe_func("match"):
		OS.alert("3.0 dev (f8a7c46 / 2017-01-11 / Bytecode version: 12)")
		return
	if _probe_func("to_json"):
		OS.alert("3.0 dev (62273e5 / 2017-01-08 / Bytecode version: 12)")
		return
	if _probe_func("path"):
		OS.alert("3.0 dev (8b912d1 / 2017-01-08 / Bytecode version: 11)")
		return
	if _probe_func("colorn") and _probe_func("rpc"):
		OS.alert("3.0 dev (23381a5 / 2016-12-17 / Bytecode version: 11)")
		return
	if _probe_func("char"):
		OS.alert("3.0 dev (513c026 / 2016-10-03 / Bytecode version: 11)")
		return
	if _probe_func("enum") and _probe_func("rpc"):
		OS.alert("3.0 dev (4ee82a2 / 2016-08-27 / Bytecode version: 11)")
		return
	if _probe_func("rpc"):
		OS.alert("3.0 dev (1add52b / 2016-08-19 / Bytecode version: 11)")
		return
	if _probe_func("enum"):
		OS.alert("2.1.3 - 2.1.6 release (ed80f45 / 2017-04-06 / Bytecode version: 10)")
		return
	if _probe_func("colorn"):
		OS.alert("2.1.2 release (85585c7 / 2017-01-12 / Bytecode version: 10)")
		return
	if _probe_func("type_exists"):
		OS.alert("2.1.0 - 2.1.1 release (7124599 / 2016-06-18 / Bytecode version: 10)")
		return
	if _probe_func("var2bytes"):
		OS.alert("2.0.0 - 2.0.4-1 release (23441ec / 2016-01-02 / Bytecode version: 10)")
		return
	if _probe_func("pi"):
		OS.alert("2.0 dev (6174585 / 2016-01-02 / Bytecode version: 9)")
		return
	if _probe_func("color8"):
		OS.alert("2.0 dev (64872ca / 2015-12-31 / Bytecode version: 8)")
		return
	if _probe_func("bp"):
		OS.alert("2.0 dev (7d2d144 / 2015-12-29 / Bytecode version: 7)")
		return
	if _probe_func("onready"):
		OS.alert("2.0 dev (30c1229 / 2015-12-28 / Bytecode version: 6)")
		return
	if _probe_func("signal"):
		OS.alert("2.0 dev (48f1d02 / 2015-06-24 / Bytecode version: 5)")
		return
	if _probe_func("OS.alerts"):
		OS.alert("1.1.0 release (65d48d6 / 2015-05-09 / Bytecode version: 4)")
		return
	if _probe_func("instance_from_id"):
		OS.alert("1.1 dev (be46be7 / 2015-04-18 / Bytecode version: 3)")
		return
	if _probe_func("var2str"):
		OS.alert("1.1 dev (2185c01 / 2015-02-15 / Bytecode version: 3)")
		return
	if _probe_func("setget"):
		OS.alert("1.0.0 release (e82dc40 / 2014-10-27 / Bytecode version: 3)")
		return
	if _probe_func("yield"):
		OS.alert("1.0 dev (8cab401 / 2014-09-15 / Bytecode version: 2)")
		return
	if _probe_func("hash"):
		OS.alert("1.0 dev (703004f / 2014-06-16 / Bytecode version: 2)")
		return
	if _probe_func("funcref"):
		OS.alert("1.0 dev (31ce3c5 / 2014-03-13 / Bytecode version: 2)")
		return
	OS.alert("Unknown version")