/*************************************************************************/
/* bytecode_versions.h                                                   */
/*************************************************************************/

#include "bytecode/bytecode_base.h"

#include "bytecode/bytecode_015d36d.h"
#include "bytecode/bytecode_054a2ac.h"
#include "bytecode/bytecode_0b806ee.h"
#include "bytecode/bytecode_1a36141.h"
#include "bytecode/bytecode_1add52b.h"
#include "bytecode/bytecode_1ca61a3.h"
#include "bytecode/bytecode_216a8aa.h"
#include "bytecode/bytecode_2185c01.h"
#include "bytecode/bytecode_23381a5.h"
#include "bytecode/bytecode_23441ec.h"
#include "bytecode/bytecode_30c1229.h"
#include "bytecode/bytecode_31ce3c5.h"
#include "bytecode/bytecode_3ea6d9f.h"
#include "bytecode/bytecode_48f1d02.h"
#include "bytecode/bytecode_4ee82a2.h"
#include "bytecode/bytecode_506df14.h"
#include "bytecode/bytecode_513c026.h"
#include "bytecode/bytecode_514a3fb.h"
#include "bytecode/bytecode_5565f55.h"
#include "bytecode/bytecode_5e938f0.h"
#include "bytecode/bytecode_6174585.h"
#include "bytecode/bytecode_620ec47.h"
#include "bytecode/bytecode_62273e5.h"
#include "bytecode/bytecode_64872ca.h"
#include "bytecode/bytecode_65d48d6.h"
#include "bytecode/bytecode_6694c11.h"
#include "bytecode/bytecode_703004f.h"
#include "bytecode/bytecode_7124599.h"
#include "bytecode/bytecode_7d2d144.h"
#include "bytecode/bytecode_7f7d97f.h"
#include "bytecode/bytecode_85585c7.h"
#include "bytecode/bytecode_8aab9a0.h"
#include "bytecode/bytecode_8b912d1.h"
#include "bytecode/bytecode_8c1731b.h"
#include "bytecode/bytecode_8cab401.h"
#include "bytecode/bytecode_8e35d93.h"
#include "bytecode/bytecode_91ca725.h"
#include "bytecode/bytecode_97f34a1.h"
#include "bytecode/bytecode_a3f1ee5.h"
#include "bytecode/bytecode_a56d6ff.h"
#include "bytecode/bytecode_a60f242.h"
#include "bytecode/bytecode_a7aad78.h"
#include "bytecode/bytecode_be46be7.h"
#include "bytecode/bytecode_c00427a.h"
#include "bytecode/bytecode_c24c739.h"
#include "bytecode/bytecode_c6120e7.h"
#include "bytecode/bytecode_d28da86.h"
#include "bytecode/bytecode_d6b31da.h"
#include "bytecode/bytecode_e82dc40.h"
#include "bytecode/bytecode_ed80f45.h"
#include "bytecode/bytecode_f3f05dc.h"
#include "bytecode/bytecode_f8a7c46.h"
#include "bytecode/bytecode_ff1e7cf.h"

void register_decomp_versions();
GDScriptDecomp *create_decomp_for_commit(uint64_t p_commit_hash);

struct GDScriptDecompVersion {
	uint64_t commit;
	String name;
};

static GDScriptDecompVersion decomp_versions[] = {

	{ 0xfffffff, "--- Please select bytecode version ---" },
	{ 0xfffffff, "4.0 release  (92bee43 / 2023-02-28 / Bytecode version: xx) - GDScript 2.0 (compiled mode not implemented yet)" },
	//	{ 0xfffffff, "     4.0 dev (5d6e853 / 2020-07-24 / Bytecode version: xx) - GDScript 2.0 (compiled mode is not implemented yet)" },
	//	{ 0x33b5c57, "     4.0 dev (33b5c57 / 2020-02-25 / Bytecode version: 13) - added 64-bit arrays to Variant" },
	//	{ 0x6da0eef, "     4.0 dev (6da0eef / 2020-02-23 / Bytecode version: 13) - added Vector2i, Rect2i and Vector3i to Variant" },
	//  { 0x3c00596, "     4.0 dev (3c00596 / 2020-02-21 / Bytecode version: 13) - added StringName to Variant" },
	//  { 0x69c95f4, "     4.0 dev (69c95f4 / 2020-02-20 / Bytecode version: 13) - added Callable and Signal to Variant" },
	{ 0xf3f05dc, "     4.0 dev (f3f05dc / 2020-02-13 / Bytecode version: 13) - removed `SYNC` and `SLAVE` tokens" },
	{ 0x506df14, "     4.0 dev (506df14 / 2020-02-12 / Bytecode version: 13) - removed `decimals` function" },
	{ 0xa7aad78, "3.5.0 release (a7aad78 / 2020-10-07 / Bytecode version: 13) - added `deep_equal` function (never added to 4.x)" },
	{ 0x5565f55, "3.2.0 release (5565f55 / 2019-08-26 / Bytecode version: 13) - added `ord` function" },
	{ 0x6694c11, "     3.2 dev (6694c11 / 2019-07-20 / Bytecode version: 13) - added `lerp_angle` function" },
	{ 0xa60f242, "     3.2 dev (a60f242 / 2019-07-19 / Bytecode version: 13) - added `posmod` function" },
	{ 0xc00427a, "     3.2 dev (c00427a / 2019-06-01 / Bytecode version: 13) - added `move_toward` function" },
	{ 0x620ec47, "     3.2 dev (620ec47 / 2019-05-01 / Bytecode version: 13) - added `step_decimals` function" },
	{ 0x7f7d97f, "     3.2 dev (7f7d97f / 2019-04-29 / Bytecode version: 13) - added `is_equal_approx` and `is_zero_approx` functions" },
	{ 0x514a3fb, "3.1.1 release (514a3fb / 2019-03-19 / Bytecode version: 13) - added `smoothstep` function, add 2nd optional arg to `var2bytes` and `bytes2var`" },
	{ 0x1a36141, "3.1.0 release (1a36141 / 2019-02-20 / Bytecode version: 13) - removed `DO`, `CASE`, `SWITCH` tokens" },
	{ 0x1ca61a3, "     3.1 beta 1 - beta 5 (1ca61a3 / 2018-10-31 / Bytecode version: 13) - added `push_error`, `push_warning` function" },
	{ 0xd6b31da, "     3.1 dev (d6b31da / 2018-09-15 / Bytecode version: 13) - added `PUPPET` token, token `SLAVESYNC` renamed to `PUPPETSYNC`" },
	{ 0x8aab9a0, "     3.1 dev (8aab9a0 / 2018-07-20 / Bytecode version: 13) - added `AS`, `VOID`, `FORWARD_ARROW` tokens" },
	{ 0xa3f1ee5, "     3.1 dev (a3f1ee5 / 2018-07-15 / Bytecode version: 13) - added `CLASS_NAME` token" },
	{ 0x8e35d93, "     3.1 dev (8e35d93 / 2018-05-29 / Bytecode version: 12) - added `REMOTESYNC`, `MASTERSYNC`, `SLAVESYNC` tokens" },
	{ 0x3ea6d9f, "     3.1 dev (3ea6d9f / 2018-05-28 / Bytecode version: 12) - added `print_debug` function" },
	{ 0xa56d6ff, "     3.1 dev (a56d6ff / 2018-05-17 / Bytecode version: 12) - added `get_stack` function" },
	{ 0xff1e7cf, "     3.1 dev (ff1e7cf / 2018-05-07 / Bytecode version: 12) - added `is_instance_valid` function" },
	{ 0x054a2ac, "3.0.0 - 3.0.6 release (054a2ac / 2017-11-20 / Bytecode version: 12) - added `polar2cartesian`, `cartesian2polar` functions" },
	{ 0x91ca725, "     3.0 dev (91ca725 / 2017-11-12 / Bytecode version: 12) - added `CONST_TAU` token" },
	{ 0x216a8aa, "     3.0 dev (216a8aa / 2017-10-13 / Bytecode version: 12) - added `wrapi`, `wrapf` functions" },
	{ 0xd28da86, "     3.0 dev (d28da86 / 2017-08-18 / Bytecode version: 12) - added `inverse_lerp`, `range_lerp` functions" },
	{ 0xc6120e7, "     3.0 dev (c6120e7 / 2017-08-07 / Bytecode version: 12) - added `len` function" },
	{ 0x015d36d, "     3.0 dev (015d36d / 2017-05-27 / Bytecode version: 12) - added `IS` token" },
	//	{ 0x5b3709d, "     3.0 dev (5b3709d / 2017-05-20 / Bytecode version: 12) - removed InputEvent variant type" },
	//	{ 0x98a3296, "     3.0 dev (98a3296 / 2017-05-17 / Bytecode version: 12) - removed Image variant type" },
	{ 0x5e938f0, "     3.0 dev (5e938f0 / 2017-02-28 / Bytecode version: 12) - added `CONST_INF`, `CONST_NAN` tokens" },
	{ 0xc24c739, "     3.0 dev (c24c739 / 2017-01-20 / Bytecode version: 12) - added `WILDCARD` token" },
	{ 0xf8a7c46, "     3.0 dev (f8a7c46 / 2017-01-11 / Bytecode version: 12) - added `MATCH` token" },
	{ 0x62273e5, "     3.0 dev (62273e5 / 2017-01-08 / Bytecode version: 12) - added `validate_json`, `parse_json`, `to_json` function" },
	{ 0x8b912d1, "     3.0 dev (8b912d1 / 2017-01-08 / Bytecode version: 11) - added `DOLLAR` token" },
	{ 0x23381a5, "     3.0 dev (23381a5 / 2016-12-17 / Bytecode version: 11) - added `ColorN` functio" },
	{ 0x513c026, "     3.0 dev (513c026 / 2016-10-03 / Bytecode version: 11) - added `char` function" },
	{ 0x4ee82a2, "     3.0 dev (4ee82a2 / 2016-08-27 / Bytecode version: 11) - added `ENUM` token" },
	{ 0x1add52b, "     3.0 dev (1add52b / 2016-08-19 / Bytecode version: 11) - added `REMOTE`, `SYNC`, `MASTER`, `SLAVE` tokens" },
	{ 0xed80f45, "2.1.3 - 2.1.6 release (ed80f45 / 2017-04-06 / Bytecode version: 10) - added `ENUM` token (backport)" },
	{ 0x85585c7, "2.1.2 release (85585c7 / 2017-01-12 / Bytecode version: 10) - added `ColorN` function (backport)" },
	{ 0x7124599, "2.1.0 - 2.1.1 release (7124599 / 2016-06-18 / Bytecode version: 10) - added `type_exists` function" },
	{ 0x23441ec, "2.0.0 - 2.0.4-1 release (23441ec / 2016-01-02 / Bytecode version: 10) - added `var2bytes`, `bytes2var` functions" },
	{ 0x6174585, "     2.0 dev (6174585 / 2016-01-02 / Bytecode version: 9) - added `CONST_PI` token" },
	{ 0x64872ca, "     2.0 dev (64872ca / 2015-12-31 / Bytecode version: 8) - added `Color8` function" },
	{ 0x7d2d144, "     2.0 dev (7d2d144 / 2015-12-29 / Bytecode version: 7) - added `BREAKPOINT` token" },
	{ 0x30c1229, "     2.0 dev (30c1229 / 2015-12-28 / Bytecode version: 6) - added `ONREADY` token" },
	{ 0x48f1d02, "     2.0 dev (48f1d02 / 2015-06-24 / Bytecode version: 5) - added `SIGNAL` token" },
	{ 0x65d48d6, "1.1.0 release (65d48d6 / 2015-05-09 / Bytecode version: 4) - added `prints` function" },
	{ 0xbe46be7, "     1.1 dev (be46be7 / 2015-04-18 / Bytecode version: 3) - function `get_inst` renamed to `instance_from_id`" },
	{ 0x97f34a1, "     1.1 dev (97f34a1 / 2015-03-25 / Bytecode version: 3) - added `seed`, `get_inst` function" },
	{ 0x2185c01, "     1.1 dev (2185c01 / 2015-02-15 / Bytecode version: 3) - added `var2str`, `str2var` functions" },
	{ 0xe82dc40, "1.0.0 release (e82dc40 / 2014-10-27 / Bytecode version: 3) - added `SETGET` token" },
	{ 0x8cab401, "     1.0 dev (8cab401 / 2014-09-15 / Bytecode version: 2) - added `YIELD` token" },
	{ 0x703004f, "     1.0 dev (703004f / 2014-06-16 / Bytecode version: 2) - added `hash` function" },
	{ 0x31ce3c5, "     1.0 dev (31ce3c5 / 2014-03-13 / Bytecode version: 2) - added `funcref` function" },
	{ 0x8c1731b, "     1.0 dev (8c1731b / 2014-02-15 / Bytecode version: 2) - added `load` function" },
	{ 0x0b806ee, "     1.0 dev (0b806ee / 2014-02-09 / Bytecode version: 1) - initial version" },
	{ 0x0000000, "-NULL-" },
};
