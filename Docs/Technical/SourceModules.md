# Source Modules

> **_NOTE:_** There is the potential for some confusion between "Source Modules" and "Texture Set Modules", so we'll clairify:
> - **Source Modules** are unreal source code modules that reside in a plugin and are defined by a *.Build.cs and 
> implement a subclass of `IModuleInterface`.
> - **Texture Set Modules** add extensibility to texture sets, sub-class `UTextureSetModule`, and are instanced in the 
> module list of Texture Set Definitions.

The plugin is composed of a number of source modules, namely:
- Texture Sets Common
- Texture Sets Compiler
- Texture Sets Material Builder
- Texture Sets
- Texture Sets Editor
- Texture Sets Standard Modules

## Texture Sets Common

- **Type:** Runtime
- **Depends on:** None

Used by all other modules. Defines common interface and data structures. Code in this module

## Texture Sets Compiler

- **Type:** UncookedOnly
- **Depends on:**
  - Texture Sets Common

Contains functionality for creating derived data for texture sets. The texture set compiler consumes source texture and 
texture set data via the processing graph and packing definitions to produce derived data ready to be consumed by the 
game.

## Texture Sets Material Builder

- **Type:** UncookedOnly
- **Depends on:**
  - Texture Sets Common

Contains all functionality for building material gra

## Texture Sets

- **Type:** Runtime
- **Depends on:** 
  - Texture Sets Common
  - Texture Sets Compiler
  - Texture Sets Material Builder

Contains the "main" texture sets classes, and serves to tie together all the various aspects of the texture sets.

The majority of "engine plumbing" happens in this class, such as checking for changes, triggering compilation or 
rebuilding materials, and notifying dependencies of updates.

## Texture Sets Editor

- **Type:** Editor
- **Depends on:** 
  - Texture Sets Common
  - Texture Sets

All editor code lives here. Primarily for custom property editor UI, thumbnail generation, etc.

> **_NOTE:_** The editor is defined as the unreal editor, and not "non-runtime". Any code for managing uncooked data
> should live in an appropriate "UncookedOnly" module, and not in the editor, as it is possible to run with uncooked 
> data outside the editor context (commandlet, standalone game, etc.)

## Texture Sets Standard Modules

- **Type:** UncookedOnly
- **Depends on:** 
  - Texture Sets Common
  - Texture Sets Compiler
  - Texture Sets Material Builder
  - Texture Sets

Contains modules (subclasses of `UTextureSetModule`) that provides a set of standardized functionality across all users
of texture sets.

If you're looking to implement your own texture set modules, these standard modules provide great examples and reference.

Details on the functionality of each of the standard module can be found on the [Standard Modules](./StandardModules.md) page.