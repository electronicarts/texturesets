# Installing Texture Sets

Texture Sets does not distribute binary builds, and requires some minor engine changes to function.

## Building Texture Sets

As Texture Sets does not distribute binary builds, you are required to build the plugin yourself. Including the plugin in the Engine/Plugins folder, or in your project's plugins folder will cause it to build.

> **_NOTE:_** Texture sets will fail to compile if the required engine changes are not implemented beforehand.

## Evaluating Texture Sets

It's likely that you'll want to try out Texture Sets before going through the work of integrating it into your project. Forks of the engine with the required changes can be found at: https://github.com/MartinPalko/UnrealEngine in the branches beginning with 'texture-sets/' (Accessible to Unreal Engine licensees with a linked Github account) There will be one branch for each supported major release of the engine. (e.g. [texture-sets/5.5](https://github.com/MartinPalko/UnrealEngine/tree/texture-sets/5.5))

These branches also include a submodule of the appropriate revision of the Texture Set plugin, so you should be able to simply check out and build the branch to begin working with the Texture Sets!

## Integrating Engine Changes

All required engine changes are wrapped in code tags to facilitate tracking. Code tags look like:

```
// Unmodified engine code...

// TEXTURESET_START - {date} - {user} - [DIVERGENCE] {description}
// Modified engine code
// TEXTURESET_END

// ... Unmodified engine code
```

If you've already checked out an engine fork, searching the codebase for `TEXTURESET_START` will allow you to find all instances of engine changes.

You can also compare the fork to the base engine via [github's web ui](https://github.com/EpicGames/UnrealEngine/compare/5.5...MartinPalko:UnrealEngine:texture-sets/5.5), for a quick overview.

> **_NOTE:_** We don't have branches for all engine versions, but that doesn't mean you can't use Texture Sets! It may just require some additional work to port to your specific engine version.