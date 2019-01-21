## Godot RE Tools

This software in a pre-alpha stage and is not suitable for use in production.

#### Introduction

This module includes following tools:

- PCK archive explorer/tester/extractor/creator.
- GDScript batch "compiler"/"decompiler".
- Resource text <-> binary batch converter.

![Screenshot](screenshot.png)

#### Requirements

Godot 3.1

#### GDScript compatibility

Different GDScript byte-code versions are incompatible and there's no reliable way to distinguish them. To successfuly decompile `.gdc`/`.gde` files compiled with specific version of Godot engine use this module with exactly same version of the engine.

#### Downloading and compiling

Clone this repository into Godots `modules` subfolder as `gdsdecomp`.
Rebuild Godot engine as described in https://docs.godotengine.org/en/latest/development/compiling/index.html.
