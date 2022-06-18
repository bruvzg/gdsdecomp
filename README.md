# Godot RE Tools

*This software in a pre-alpha stage and is not suitable for use in production.*

## Introduction

![Code Screenshot](screenshot.png)

This module includes following tools:

- PCK archive explorer / tester / extractor / creator.
- GDScript batch "compiler" / "decompiler".
- Resource text <-> binary batch converter.
- Full project recovery (CLI-only)

Full project recovery performs the following:
- Decompiles all GDScript scripts
- Recovers the original project file
- Converts all resources back to their original import formats
 - Does not currently support converting models and ADPCM wavs to their original format

This module has support for decompiling Godot 4.x, 3.x, and 2.x projects.

## Usage

- To perform full project recovery from the command line:
```bash
gdre_tools --extract=game.pck --output-dir=game-extract/
```

Use the same Godot tools version that the original game was compiled in to edit the project. In order to detect this, see [To detect Godot version](#to-detect-godot-version)

- All other module functions (excepting for full project recovery) are accessible via `RE Tools` menu.
- Decompilers additionally available as `GDScriptDecomp_{commit_hash}` GDScript classes.

```gdscript
	var decomp = GDScriptDecomp_{commit_hash}.new()

	decomp.decompile_byte_code(path)
	# or
	decomp.decompile_byte_code_encrypted(path, key)

	var source = decomp.get_script_text()
```

## Requirements

Godot 4.0 (master branch) @ commit https://github.com/godotengine/godot/commit/ff75a49227d8d81f99b728fc89b1e4e6e79ed5be
- Support for building on 3.x has been dropped and no new features are being pushed.

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

Clone this repository into Godot's `modules` subfolder as `gdsdecomp`.
Rebuild Godot engine as described in https://docs.godotengine.org/en/latest/development/compiling/index.html.

## License

The source code of the module is licensed under MIT license.

[![Travis Build Status](https://travis-ci.org/bruvzg/gdsdecomp.svg?branch=master)](https://travis-ci.org/bruvzg/gdsdecomp)
