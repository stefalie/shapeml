# TODO List and Improvement Ideas

## Rendering

- [ ] Use a better sky model, for example the [Preetham model based on Spherical Harmonics](https://www.cg.tuwien.ac.at/research/publications/2008/Habel_08_SSH/).
- [ ] Frustum Culling
- [ ] [Tiled and/or clustered renderer](http://www.aortiz.me/2018/12/21/CG.html)
- [ ] Use a [reverse Z-Buffer](https://developer.nvidia.com/content/depth-precision-visualized) and maybe put [the far plane to infinity](http://dev.theomader.com/depth-precision/).
  This will be quite painful as it entails a cascade of changes that need to be propagated through the rendering code and the shaders.
- [ ] MSAA for the deferred renderer (tricky to get right).
- [ ] MSAA for transparency, necessary to hide gaps between opaque objects in front of transparent objects.
  MSAA resolving for LFB and adaptive transparency (Salvi, Montgomery, Lefohn, 2011) as described in Section 4.2 of the paer.
  This [explanation](https://stackoverflow.com/questions/16934695/order-independent-transparency-with-msaa) and these [slides](http://developer.amd.com/wordpress/media/2013/06/2041_final.pdf) will help.
  It will require storing the coverage mask in the per-pixel fragment lists (2 bits for 4 samples should suffice).
  For the LFB version, the blit to a non-MSAA target will happen before sorting and resolving (resolving will take the coverage into account).
  For adaptive transparency, blit happens after resolving.
  For OIT it should be straight forward (just render it with MSAA active).
- [ ] Rendering modes for wireframe, face/vertex normals, and scope axes and sizes.
- [ ] An idea for transparency rendering with approximate sorting (could work depending on the scene): make the sort key in the current renderer a union and use a different bitfield for transparent object (one where the depth has a higher priority than mesh or texture IDs).
- [ ] There are a lot of boolean flags in the shader to trigger options.
  Use specialization constants instead or `#define`s plus shader variants.
- [ ] Add a depth buffer guard band for SAO to also work at the screen boundaries (described in the paper).
- [ ] Experiment with [Line-Sweep Ambient Obscurance](http://wili.cc/research/lsao/) and/or [Screen-Space Far-Field Ambient Obscurance](http://wili.cc/research/ffao/).
- [ ] Give Ground Truth Ambient Occlusion (GTAO) a try (it's an improvement and follow-up of Line-Sweep Ambient Obscurance mentioned above).
  See *Jimenez et al., 2016, Practical Realtime Strategies for Accurate Indirect Occlusion*.
- [ ] Try a [guided filter](https://bartwronski.com/2019/09/22/local-linear-models-guided-filter/) for removing noise in SSAO.
  The mentioned upsampling approach is also interesting (and doing SSAO at lower resolution makes sense, especially for high DPI monitors).
- [ ] Add an invisible guard band margin to the framebuffer to prevent SSAO artifacts at the window edges (as described in the SAO paper).
- [ ] Try [HBAO+](https://www.geforce.co.uk/hardware/technology/hbao-plus/technology) and compare it to SAO.
  Especially the idea of using an interleaved rendering approach with jittered samples to avoid cache trashing seems interesting.
- [ ] An FPS camera mode.
  A select shape (i.e., picking) and set focus on function for the trackball.
- [ ] Separate shader objects and shader program objects more so that shader objects can be used by several programs.
  The fullscreen triangle shader for example is used (and separately compiled) by ~10 different programs.
- [ ] TAA (temporal anti-aliasing)
- [ ] [DDGI (Dynamic Diffuse Global Illumination)](https://morgan3d.github.io/articles/2019-04-01-ddgi/) (probably out of scope for this project).
  See also this [follow-up article](https://arxiv.org/abs/2009.10796).
- [ ] Virtual Shadow Maps (see papers [one](https://efficientshading.com/2014/01/01/efficient-virtual-shadow-maps-for-many-lights/) and [two](https://efficientshading.com/2015/06/01/more-efficient-virtual-shadow-maps-for-many-lights/)).
- [ ] Try contact shadows aka [screen space shadows](https://panoskarabelas.com/posts/screen_space_shadows/) (ray trace/march against depth buffer towards light).
  It should help reducing biasing and Peter panning artifacts.

## ShapeML/ShapeMaker Features

- [ ] Allow for CSG operations between shapes (this will get nasty and is out of scope for this project).
- [ ] Expose all settings in ShapeMaker as command line options (to make it scriptable via command line).
  Mostly the render settings are missing.
  Instead of having a million command line options, maybe it's better to use a Json config file.
- [ ] Better test the FFD (free form deformation) functionality.
- [ ] Better test UV projection functionality.
- [ ] Support for more shape operation:
  - [ ] decimation
  - [ ] triangulation
  - [ ] geometry cleanup (merging co-planar faces, co-linear edges, and duplicate vertices)
- [ ] Intersection queries should work on the exact meshes and not just on the bounding box, or it should be possible to choose what kind of query one wants.
  To do such exact queries, we'd need to build a BVH (of AABBs most likely since we already have them) to have decent performance.
  For this to work correct, one also needs general polygon-polygon intersection tests (just triangle-triangle tests won't be enough).
  Good information to start working on this can be found [here](https://stackoverflow.com/questions/4632951/kdtree-splitting) or in *Wald, 2007, On Fast Construction of SAH-based Bounding Volume Hierarchies*.
- [ ] Consider functions and constants having conditions and probabilities too.
- [ ] Consider adding rule priorities, be it as global state or as per rule option.
- [ ] Consider adding functionality for grouping parameters inside ShapeMaker, either as global state or per parameter.
  It could be done with a prefix of parameters names, e.g., everything before the first underscore is the group name (as in CMake).
- [ ] There are currently two ways to display a ground plane (either through the global ShapeMaker settings by adding a renderable or in the background settings by enabling the infinite ground plane shader.
  Pick one.
- [ ] Add a graph-based shape tree viewer to ShapeMaker.
- [ ] `rotateScope` shape operations should also be allowed on empty shapes (without meshes).
- [ ] `roofShed` should support an overhang parameter like the other roof operations.
- [ ] An `--output` option for ShapeMaker to specify the file name for exports.
- [ ] vim-shapeml only has syntax coloring but unfortunately no indentation script. Make one.

## Implementation Issues

- [ ] Try to avoid unnecessary mesh copying the following way (it currently won't work because `shape->mesh()` returns a `const`s `shared_ptr`, the `const` part would need to be cast away): `geometry::HalfedgeMeshPtr tmp = shape->mesh().use_count() > 1 ? std::make_shared<geometry::HalfedgeMesh>(*shape->mesh()) : shape->mesh();`.
- [ ] Stop abusing `std::shared_ptr` (this concerns all uses of it in ShapeML).
- [ ] Stop abusing exceptions in the parser and interpreter for control flow.
- [ ] Not all built-in mesh types are cached in the `AssetCache`.
  Consider adding the missing ones even though it might be tricky to come up with a good keys for them (what's a good unique key to describe an arbitrary polygon?).
  It might still be worth it because it will enable instancing for these meshes.
  It's probably the best to `reinterpret_cast` the floating point values to int and to use these in the key.
- [ ] Check all the `TODO` tags in the source code ;-).

## More Example Grammars
- [ ] A complicated facade such as the facade of the Louvre in Paris. See Fig. 6.7 in *Zmugg, 2015, Procedural Creation of Man-Made Shapes and Structures*.
- [ ] More buildings. Inspirations:
  - [https://twitter.com/gkurkdjian/status/1266304148493889542](https://twitter.com/gkurkdjian/status/1266304148493889542)
  - [http://microbot.ch/new/procedural-building-modules](http://microbot.ch/new/procedural-building-modules/)
  - [https://twitter.com/3DCrede/status/1049980458610688000](https://twitter.com/3DCrede/status/1049980458610688000)
  - [https://twitter.com/knosvoxel/status/1028307148084838402](https://twitter.com/knosvoxel/status/1028307148084838402)
  - [https://twitter.com/wilnyl/status/1248987005633134592](https://twitter.com/wilnyl/status/1248987005633134592)
- [ ] Something Sci-Fiy or Cyberpunky, inspirations:
  - [Cloudpunk](https://store.steampowered.com/app/746850/Cloudpunk/)
  - [https://twitter.com/Sir_carma/status/800363381744082948](https://twitter.com/Sir_carma/status/800363381744082948)
- [ ] Another castle or medieval town in low poly. Inspirations:
  - Reichsburg Cochem
  - [Teutonic Fortress](https://3dwarehouse.sketchup.com/model/ed35cc663b28ebcf767b959cddc3d22e/Large-Teutonic-Fortress)
  - Massive Castle [Part 1](https://3dwarehouse.sketchup.com/model/966cba532acb655b96aa5dc23c036c/Massive-castle-part-I), [Part 2](https://3dwarehouse.sketchup.com/model/9b023d201c425ad4b96aa5dc23c036c/Massive-Castle-part-II)
  - [https://twitter.com/TimSpaninks/status/1285643183385452551](https://twitter.com/TimSpaninks/status/1285643183385452551)
  - [https://twitter.com/TocoGamescom/status/947911205146030080](https://twitter.com/TocoGamescom/status/947911205146030080)
  - [https://twitter.com/beastochahin/status/1046705267012907008](https://twitter.com/beastochahin/status/1046705267012907008)
  - [https://twitter.com/scoobyhistory/status/1316470807938437121](https://twitter.com/scoobyhistory/status/1316470807938437121)
  - [https://twitter.com/danialrashidi/status/1320691588088111106](https://twitter.com/danialrashidi/status/1320691588088111106)
  - [https://twitter.com/abidaker/status/1322613530366017541](https://twitter.com/abidaker/status/1322613530366017541)
  - [https://twitter.com/ianmcque/status/1236385022401183744](https://twitter.com/ianmcque/status/1236385022401183744)

