# Godot RE Tools

*This software in a pre-alpha stage and is not suitable for use in production.*

## Introduction

![Code Screenshot](screenshot.png)

This module includes following tools:

- PCK archive explorer / tester / extractor / creator.
- GDScript batch "compiler" / "decompiler".
- Resource text <-> binary batch converter.

## Usage

- All module functions are accessible via `RE Tools` menu.
- Decompilers additionaly available as `GDScriptDecomp_{VERSION}` GDScript classes.

```gdscript
	var decomp = GDScriptDecomp_3_1_0.new()

	decomp.decompile_byte_code(path)
	# or
	decomp.decompile_byte_code_encrypted(path, key)

	var source = decomp.get_script_text()
```

## Requirements

Godot 3.1

## GDScript decompiler compatibility

#### Supported bytecode versions

- Bytecode version: 13, Godot 3.1.0;

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

> To decompile scripts from development version of Godot engine look for changes in [GDScriptFunctions::_names](https://github.com/godotengine/godot/blob/master/modules/gdscript/gdscript_tokenizer.h) and [GDScriptTokenizer::Token](https://github.com/godotengine/godot/blob/master/modules/gdscript/gdscript_tokenizer.h) and modify `func_names` array and `Token` enum in the `modules/gdsdecomp/bytecode/bytecode_{VERSION}.cpp` to match these changes.

## Downloading and compiling

Clone this repository into Godots `modules` subfolder as `gdsdecomp`.
Rebuild Godot engine as described in https://docs.godotengine.org/en/latest/development/compiling/index.html.

## License

The source code of the module is licensed under MIT license.
