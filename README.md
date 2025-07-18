# Black Key


A realtime rendering engine leveraging modern vulkan and C++ features built as a research project.

All the following scenes are rendered in engine:


![Sponza](images/sponza.png)
![Normal](images/normal.png)
![Bistro](images/bistro.png)

## Currently supported features

# Graphics
* Physically based rendering using a cook-torrence brdf
* Image based lighting
* Cascaded shadow maps + PCF filtering
* GLTF loading support via Fastgltf
* Early depth testing via Z-prepass
* HDR + options for Filmic/Uncharted/unreal tonemappers
* CPU side Frustum culling
* Multisampling anti-aliasing
* Normal mapping
* Ktx Texture support for skybox
* Transparency
* FXAA

# API Features
* Buffer device addressing allowing programmable vertex pulling
* Dynamic Rendering
* Bindless resources via descriptor indexing

##  Roadmap
* [x] Clustered forward shading
* [x] GPU driven rendering
* [x] Move Frustum culling to a compute shader
* [x] occlusion culling
* [ ] Bloom
* [ ] HBAO/GTAO/SSAO
* [ ] Global illumination(SSGI(HBIL/SSIL)/Voxel GI/Radiance Cascades/SDFDDGI)


## Things to look into
* Render Graph
* Async compute

