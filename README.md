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
- Decompilers additionaly available as `GDScriptDecomp_{commit_hash}` GDScript classes.

```gdscript
	var decomp = GDScriptDecomp_{commit_hash}.new()

	decomp.decompile_byte_code(path)
	# or
	decomp.decompile_byte_code_encrypted(path, key)

	var source = decomp.get_script_text()
```

## Requirements

Godot 3.1, 3.2 or 4.0

## GDScript decompiler compatibility

[Supported bytecode versions](BYTECODE_HISTORY.md)

To decompile GDScript, exact engine version that was used to compile it, should be specified.

### To detect Godot version 

Run `./{executable_name} --version > version.txt`.

Version strings have following formats:
- Official releases: `{major}.{minor}.{rev}.stable.official`
- Development builds:`{major}.{minor}.{rev}.{build_type}.{commit_hash}`

### To detect GDScript version

Copy executable to the `helpers` folder.
Run `./{executable_name} -s detect_bytecode_ver.gd --path .`.

## Downloading and compiling

Clone this repository into Godots `modules` subfolder as `gdsdecomp`.
Rebuild Godot engine as described in https://docs.godotengine.org/en/latest/development/compiling/index.html.

## License

The source code of the module is licensed under MIT license.

[![Travis Build Status](https://travis-ci.org/bruvzg/gdsdecomp.svg?branch=master)](https://travis-ci.org/bruvzg/gdsdecomp)
