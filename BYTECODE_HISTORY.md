## GDScript bytecode versions

- Bytecode version: 13, Godot 3.1.0 (master);

> *Tokens removed:* `DO`, `SWITCH`, `CASE`

- Bytecode version: 13, Godot 3.1.0 (beta 1 - beta 5);

> *Tokens added:* `PUPPET`, `REMOTESYNC`, `MASTERSYNC`, `PUPPETSYNC`, `AS`, `VOID`, `FORWARD_ARROW`, `CLASS_NAME`
>
> *Functions added:* `is_instance_valid`, `get_stack`, `print_debug`, `push_error`, `push_warning`

- Bytecode version: 12, Godot 3.0.0 - 3.0.6;

> *Tokens added:* `MATCH`, `IS`, `REMOTE`, `SYNC`, `MASTER`, `SLAVE`, `DOLLAR`, `CONST_TAU`, `WILDCARD`, `CONST_INF`, `CONST_NAN`
>
> *Functions added:* `inverse_lerp`, `range_lerp`, `polar2cartesian`, `cartesian2polar`, `wrapi`, `wrapf`, `char`, `validate_json`, `parse_json`, `to_json`, `len`

- Bytecode version: 10, Godot 2.1.3 - 2.1.5;

> *Tokens added:* `ENUM`

- Bytecode version: 10, Godot 2.1.2;

> *Functions added:* `ColorN`

- Bytecode version: 10, Godot 2.1.0 - 2.1.1;

> *Functions added:* `type_exists`

- Bytecode version: 10, Godot 2.0.0 - 2.0.4-1;

> *Tokens added:* `ONREADY`, `SIGNAL`, `BREAKPOINT`, `CONST_PI`
>
> *Functions added:* `var2bytes`, `bytes2var`, `Color8`

- Bytecode version: 4, Godot 1.1.0;

> *Functions added:* `seed`, `prints`, `var2str`, `str2var`, `instance_from_id`

- Bytecode version: 3, Godot 1.0.0

> *Tokens added:* `TK_PR_SETGET`, `TK_PR_YIELD`
>
> *Functions added:* `funcref`, `hash`

- Bytecode version: 1, Initial version (commit b2ce682)
