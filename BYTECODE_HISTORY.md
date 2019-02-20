## GDScript bytecode versions

| Release         | Commit  | Date         | Bytecode | Changes                                                         |
| ------------------------ | ------- | ------------ | -----------------| --------------------------------------------------------------- |
| 1.0 dev                  | [0b806ee](https://github.com/godotengine/godot/commit/0b806ee) | `2014.02.09` | 1                |                                                                 |
| 1.0 dev                  | [8c1731b](https://github.com/godotengine/godot/commit/8c1731b) | `2014.02.15` | 2                | Added `load` function                                           |
| 1.0 dev                  | [31ce3c5](https://github.com/godotengine/godot/commit/31ce3c5) | `2014.03.13` | 2                | Added `funcref` function                                        |
| 1.0 dev                  | [703004f](https://github.com/godotengine/godot/commit/703004f) | `2014.06.16` | 2                | Added `hash` function                                           |
| 1.0 dev                  | [8cab401](https://github.com/godotengine/godot/commit/8cab401) | `2014.09.15` | 2                | Added `YIELD` token                                             |
| **1.0.0 release**            | [e82dc40](https://github.com/godotengine/godot/commit/e82dc40) | `2014.10.27` | 3                | Added `SETGET` token                                            |
| 1.1 dev                  | [2185c01](https://github.com/godotengine/godot/commit/2185c01) | `2015.02.15` | 3                | Added `var2str`, `str2var` functions                            |
| 1.1 dev                  | [97f34a1](https://github.com/godotengine/godot/commit/97f34a1) | `2015.03.25` | 3                | Added `seed`, `get_inst` function                               |
| 1.1 dev                  | [be46be7](https://github.com/godotengine/godot/commit/be46be7) | `2015.04.18` | 3                | Function `get_inst` renamed to `instance_from_id`               |
| **1.1.0 release**            | [65d48d6](https://github.com/godotengine/godot/commit/65d48d6) | `2015.05.09` | 4                | Added `prints` function                                         |
| 2.0 dev                  | [48f1d02](https://github.com/godotengine/godot/commit/48f1d02) | `2015.06.24` | 5                | Added `SIGNAL` token                                            |
| 2.0 dev                  | [30c1229](https://github.com/godotengine/godot/commit/30c1229) | `2015.12.28` | 6                | Added `ONREADY` token                                           |
| 2.0 dev                  | [7d2d144](https://github.com/godotengine/godot/commit/7d2d144) | `2015.12.29` | 7                | Added `BREAKPOINT` token                                        |
| 2.0 dev                  | [64872ca](https://github.com/godotengine/godot/commit/64872ca) | `2015.12.31` | 8                | Added `Color8` function                                         |
| 2.0 dev                  | [6174585](https://github.com/godotengine/godot/commit/6174585) | `2016.01.02` | 9                | Added `CONST_PI` token                                          |
| **2.0.0 release**            | [23441ec](https://github.com/godotengine/godot/commit/23441ec) | `2016.01.02` | 10               | Added `var2bytes`, `bytes2var` functions                        |
| **2.1.0 release** / 3.0-dev  | [7124599](https://github.com/godotengine/godot/commit/7124599) | `2016.06.18` | 10               | Added `type_exists` function                                    |
| 3.0 dev                  | [1add52b](https://github.com/godotengine/godot/commit/1add52b) | `2016.08.19` | 11               | Added `REMOTE`, `SYNC`, `MASTER`, `SLAVE` tokens                |
| 3.0 dev                  | [4ee82a2](https://github.com/godotengine/godot/commit/4ee82a2) | `2016.08.27` | 11               | Added `ENUM` token                                              |
| 3.0 dev                  | [513c026](https://github.com/godotengine/godot/commit/513c026) | `2016.10.03` | 11               | Added `char` function                                           |
| 3.0 dev                  | [23381a5](https://github.com/godotengine/godot/commit/23381a5) | `2016.12.17` | 11               | Added `ColorN` function                                         |
| **2.1.2 release**            | [85585c7](https://github.com/godotengine/godot/commit/85585c7) | `2017.01.12` | 10               | Added `ColorN` function                                         |
| **2.1.3 release**            | [ed80f45](https://github.com/godotengine/godot/commit/ed80f45) | `2017.04.06` | 10               | Added `ENUM` token                                              |
| 3.0 dev                  | [8b912d1](https://github.com/godotengine/godot/commit/8b912d1) | `2017.01.08` | 11               | Added `DOLLAR` token                                            |
| 3.0 dev                  | [62273e5](https://github.com/godotengine/godot/commit/62273e5) | `2017.01.08` | 12               | Added `validate_json`, `parse_json`, `to_json` function         |
| 3.0 dev                  | [f8a7c46](https://github.com/godotengine/godot/commit/f8a7c46) | `2017.01.11` | 12               | Added `MATCH` token                                             |
| 3.0 dev                  | [c24c739](https://github.com/godotengine/godot/commit/c24c739) | `2017.01.20` | 12               | Added `WILDCARD` token                                          |
| 3.0 dev                  | [5e938f0](https://github.com/godotengine/godot/commit/5e938f0) | `2017.02.28` | 12               | Added `CONST_INF`, `CONST_NAN` tokens                           |
| 3.0 dev                  | [015d36d](https://github.com/godotengine/godot/commit/015d36d) | `2017.05.27` | 12               | Added `IS` token                                                |
| 3.0 dev                  | [c6120e7](https://github.com/godotengine/godot/commit/c6120e7) | `2017.08.07` | 12               | Added `len` function                                            |
| 3.0 dev                  | [d28da86](https://github.com/godotengine/godot/commit/d28da86) | `2017.08.18` | 12               | Added `inverse_lerp`, `range_lerp` functions                    |
| 3.0 dev                  | [216a8aa](https://github.com/godotengine/godot/commit/216a8aa) | `2017.10.13` | 12               | Added `wrapi`, `wrapf` functions                                |
| 3.0 dev                  | [91ca725](https://github.com/godotengine/godot/commit/91ca725) | `2017.11.12` | 12               | Added `CONST_TAU` token                                         |
| **3.0.0 release**            | [054a2ac](https://github.com/godotengine/godot/commit/054a2ac) | `2017.11.20` | 12               | Added `polar2cartesian`, `cartesian2polar` functions            |
| 3.1 dev                  | [ff1e7cf](https://github.com/godotengine/godot/commit/ff1e7cf) | `2018.05.07` | 12               | Added `is_instance_valid` function                              |
| 3.1 dev                  | [a56d6ff](https://github.com/godotengine/godot/commit/a56d6ff) | `2018.05.17` | 12               | Added `get_stack` function                                      |
| 3.1 dev                  | [3ea6d9f](https://github.com/godotengine/godot/commit/3ea6d9f) | `2018.05.28` | 12               | Added `print_debug` function                                    |
| 3.1 dev                  | [8e35d93](https://github.com/godotengine/godot/commit/8e35d93) | `2018.05.29` | 12               | Added `REMOTESYNC`, `MASTERSYNC`, `SLAVESYNC` tokens            |
| 3.1 dev                  | [a3f1ee5](https://github.com/godotengine/godot/commit/a3f1ee5) | `2018.07.15` | 13               | Added `CLASS_NAME` token                                        |
| 3.1 dev                  | [8aab9a0](https://github.com/godotengine/godot/commit/8aab9a0) | `2018.07.20` | 13               | Added `AS`, `VOID`, `FORWARD_ARROW` tokens                      |
| 3.1 dev                  | [d6b31da](https://github.com/godotengine/godot/commit/d6b31da) | `2018.09.15` | 13               | Added `PUPPET` token, token `SLAVESYNC` renamed to `PUPPETSYNC` |
| 3.1 beta 1               | [1ca61a3](https://github.com/godotengine/godot/commit/1ca61a3) | `2018.10.31` | 13               | Added `push_error`, `push_warning` function                     |
| 3.1 beta 6               | [1a36141](https://github.com/godotengine/godot/commit/1a36141) | `2019.02.20` | 13               | Removed `DO`, `CASE`, `SWITCH` tokens                           |
