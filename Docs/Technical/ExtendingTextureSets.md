# Extending Texture Sets

Texture sets can be extended by creating new modules. Texture set modules are able to:

- Define texture set input elemements
- Define texture set output elements
- Attach parameters to texture set assets
- Define processing logic during the build process
- Attach parameters to texture set samplers
- Define sampling logic

> **_NOTE:_** For examples and reference it's recommended to check out the standard modules found in the `TextureSetsStandardModules` source module in the Texture Sets plugin.

## Creating a Module

Texture set modules can be created anywhere in C++ (your own plugin, game code, etc.)

First, you must add dependencies to all the required texture sets source modules in your `*.Build.cs`:
```
...
"TextureSetsCommon",
"TextureSets",
"TextureSetsCompiler",
"TextureSetsMaterialBuilder",
...
```
> **_NOTE:_** If your module only affects the compilation, you should be able to drop the dependency on the material builder, and vice-versa.

Now, simply declare a new `UObject` class inherited from `UTextureSetModule` and begin implementing the virtual methods. Check comments on the `UTextureSetModule` for explanation of the purpose of each method, and look at the standard modules for examples.

## Texture Elements

See `UCustomElementModule::ConfigureProcessingGraph` for an example of how to add an input to the processing graph which will output without any further computation.

No manual hashing is required for the processing graph, as each node can be hashed.

## Processing Logic

Create custom processing nodes by inheriting from either `ITextureProcessingNode` or `IParameterProcessingNode`, and implementing all of the pure virtual functions. Check the function declarations' comments for details.

See `UPBRSurfaceModule` for an example of a module which does some simple processing logic.

See `UFlipbookModule` for an example of a module which does more advanced processing logic; converting texture sheet inputs into a texture array.

## Sampling Logic

Your module can generate unreal materials logic by implementing the virtual `ConfigureSamplingGraphBuilder` function. Make use of the `FTextureSetMaterialGraphBuilder` helper class which will then generate the material graph code inside a material function used by the sampler.

`UCustomElementModule::ConfigureSamplingGraphBuilder` shows how to simply output a sample of one texture output of the processing graph.

`UProceduralMappingModule`, `UFlipbookModule`, and `UHeightModule` are good examples of advanced sampling logic that can be implemented by modules.

> **_NOTE:_** Unlike the processing graph, the FTextureSetMaterialGraphBuilder does not hash it's inputs automatically. It's important you implement `UTextureSetModule::ComputeSamplingHash` in your module and hash all values that could affect the sampling graph so that it will be re-generated when needed.

## Texture Set Asset Params

Modules can attach custom meta-data to texture set assets that use them. This is useful for adding custom user exposed proerties such as the `UFlipbookModule` module does, to allow the user to set the flipbook default frame rate for a flipbooked texture set.

To attach asset params to a texture set, create a sub-class of `UTextureSetAssetParams`, and add the desired members to it with a `UPROPERTY(EditAnywhere)` makup.

In your module, implement the `GetAssetParamClass` virtual function and return the class type of your `UTextureSetAssetParams` subclass.

This will cause the texture set system to create an instance of yoru  asset params in the `FTextureSetAssetParamsCollection` of the texture set.

Processing nodes can then make use of the asset params which can be found in the `FTextureSetProcessingContext` provided to them while executing the graph.

## Texture Set Sample Params

Sample params function similairly to the asset params, but instead you subclass `UTextureSetSampleParams`, and the sample params are attached to the instance of the sampler node in the material graph.

This is used by the `UFlipbookModule` to affect graph generation depending on if the user wants to interpolate between frames or not.

The sample param is made available via the `FTextureSetAssetParamsCollection` passed to `UTextureSetModule::ComputeSamplingHash`.