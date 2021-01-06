# ShapeML Reference

This is a very minimalist reference (more a list of the built-in functionality without too much explanation). It assumes familiarity with L-Systems and/or CGA.
To get started it's better to checkout the [example grammars](../grammars).

## Citizens of ShapeML

### Parameters

Parameters can either be set via cmd (`--parameter PARAMETER_NAME=VALUE`) or interactively in the ShapeMaker Gui.

```
param number = 10;
param checkbox = false;
param my_favorite_color = "#336699";
```

### Constants (Global Variables)

```
const cos_90_deg = cos(90);
```

### Custom Functions

```
func silly_add(val1, val2) = val1 + val2;
```

### Shape Operation Strings

Shape operations strings can contain shape operations (what a surprise), rule names, dereferenced (with `^`) variables that contain other shape operation strings, and names of terminal symbols (terminal symbols always end in an underscore `_`):

```
{
  translateX(10)
  SomeCustomRule
  ^ref_to_var_containing_shape_op_str
  TerminalSymbol_
}
```

### Rules

```
// The simplest form assigns a name to a shape op string
rule SimplestRule = {
  translateX(10)
  Done_
};

// Rule that takes arguments
rule RuleWithParameter(param1) = {
  translateX(param1)
  Done_
};

// Conditional rules
param high_LOD = true;
rule LevelOfDetailRule :: high_LOD = {
  sphere(32, 32)
  RoundSphere_
};
rule LevelOfDetailRule :: !high_LOD = {
  sphere(3, 3)
  SphereWithCorners_
};

// Probabilistic rules
// 2/3 chance of creating a red cube, 1/3 chance of creating a green cube
rule RedGreenCube : 2 = {
  color(1, 0, 0)
  cube
  RedCube_;
};
rule RedGreenCube : 1 = {
  color(0, 1, 0)
  cube
  GreenCube_;
};

// The probability comes first when there is also a condition
rule ProbabilisticConditionalRule : probability :: condition = { ... };
```

## Preprocessor

```
#include "file_name.shp"
```

## Shape Operations

### Transformations

```
// Translation relative to scope origin
translate(float translation_x, float translation_y, float translation_z)
translateX(float translation_x)
translateY(float translation_y)
translateZ(float translation_z)

// Sets absolute position of scope origin
translateAbs(float translation_x, float translation_y, float translation_z)
translateAbsX(float translation_x)
translateAbsY(float translation_y)
translateAbsZ(float translation_z)

// Set scope size
size(float size_x, float size_y, float size_z)
sizeX(float size_x)
sizeY(float size_y)
sizeZ(float size_z)

// Scales scope size
scale(float factor_x, float size_y, float size_z)
scale(float factor_xyz)  // uniform scaling
scaleX(float factor_x)
scaleY(float factor_y)
scaleZ(float factor_z)

// Scales about the scope center
scaleCenter(float factor_x, float size_y, float size_z)
scaleCenter(float factor_xyz)  // uniform scaling
scaleCenterX(float factor_x)
scaleCenterY(float factor_y)
scaleCenterZ(float factor_z)

// Rotates scope + containing geometry
rotate(float axis_x, float axis_y, float axis_z, float angle)
rotateX(float angle_x)
rotateY(float angle_y)
rotateZ(float angle_z)

// Rotates only scope, containing geometry keeps its absolute orientation
rotateScope(float axis_x, float axis_y, float axis_z, float angle)
rotateScopeX(float angle_x)
rotateScopeY(float angle_y)
rotateScopeZ(float angle_z)
rotateScopeXYToXZ  // useful after face split before generating roofs

// For L-Systems
rotateZAxisHorizontal  // see page 57 of [Prusinkiewicz, Lindenmayer; 1990; The Algorithmic Beauty of Plants].
rotateHorizontal
turnYAxisToVec         // see [Talton et al.; 2011; Metropolis Procedural Modeling]
turnYAxisPerpToVec     // "
turnZAxisToVec         // "

// Centers within next shape below in the derivation stack. If there is none, it centers within parent shape.
center
centerX
centerY
centerZ
centerAtOrigin
centerAtOriginX
centerAtOriginY
centerAtOriginZ
```

### Geometry Manipulation

```
extrude(float length)
extrude(float direction_x, float direction_y, float direction_y, float length)
extrudeWorld(float direction_x, float direction_y, float direction_y, float length)  // direction in world coords

mirrorX
mirrorY
mirrorZ

normalsFlip
normalsFlat    // like flat shading
normalsSmooth  // vertex normals become average of face normals
```

### Trimming

```
trim
trimLocal  // ignores parent trim planes
trimPlane(float normal_x, float normal_y, float normal_z, float distance)
trimPlane(float normal_x, float normal_y, float normal_z, float pos_x, float pos_y, float pos_z)
```

### Free Form Deformations (FFD)

```
ffdReset(int subdivisions_x, int subdivisions_y, int subdivisions_z)
ffdTranslate(int control_point_index_x, int control_point_index_y, int control_point_index_z, float translation_x, float translation_y, float translation_z)
ffdTranslateX(int control_point_index_x, int control_point_index_y, int control_point_index_z, float translation_x)
ffdTranslateY(int control_point_index_x, int control_point_index_y, int control_point_index_z, float translation_y)
ffdTranslateZ(int control_point_index_x, int control_point_index_y, int control_point_index_z, float translation_z)
ffdApply
```

### Texture Coordinate Manipulation

```
uvScale(float size_u, float size_v)
uvTranslate(float translation_u, float translation_v)
uvSetupProjectionXY(float tex_width, float tex_height)
uvSetupProjectionXY(float tex_width, float tex_height, float offset_x, float offset_y)
uvProject
```

### Primitives and Mesh Instantiation

```
mesh(string file_path)  // only OBJ meshes supported

// 2D primitives
quad
circle(int subdivisions_phi)
shapeL(float back_width, float left_width)
shapeU(float back_width, float left_width, float right_width)
shapeT(float top_width, float vertical_width, float vertical_position)
shapeh(float left_width, float horizontal_width, float right_width, float horizontal_position)
polygon(float pos_1_x, float pos_1_y,
        float pos_2_x, float pos_2_y,
        ...
        float pos_n_x, float pos_n_y)
grid(int subdivisions_x, int subdivisions_z)
disk(int subdivisions_phi, int subdivisions_r)

// 3D primitives
cube
box(int subdivisions_x, int subdivisions_y, int subdivisions_z)
cylinder(int subdivisions_phi, int subdivisions_y, int subdivisions_radial)
cylinderFlat(int subdivisions_phi, int subdivisions_y, int subdivisions_radial)
cone(int subdivisions_phi, int subdivisions_y, int subdivisions_radial)
coneFlat(int subdivisions_phi, int subdivisions_y, int subdivisions_radial)
sphere(int subdivisions_phi, int subdivisions_theta)
sphereFlat(int subdivisions_phi, int subdivisions_theta)
torus(float radius_1, float radius_2, int subdivisions_phi, int subdivisions_theta)
torusFlat(float radius_1, float radius_2, int subdivisions_phi, int subdivisions_theta)

// Roofs
roofHip(float angle)
roofHip(float angle, float overhang)
roofGable(float angle)
roofGable(float angle, float overhang_side, float overhang_gable)
roofPyramid(float height)
roofPyramid(float height, float overhang_height)
roofShed(float angle)
```

### Materials

```
// Albedo
color(float red, float green, float blue)               // sRGB
color(float red, float green, float blue, float alpha)  // sRGB
color(string color_in_hex_format)                       // 4 or 5, or 7 or 9 chars
texture(string file_path)                               // sRGB, assumed transparent if alpha channel is present 
textureNone

// PBR, see https://github.com/google/filament for parametrization
metallic(float m)
roughness(float alpha)
reflectance(float r)

materialName(string name)
```

### Misc

```
// Debug output
print(string output_message)
printLn(string output_message)

// Add current shape to octree, can be queried via a 'occlusion' shape attribute
octreeAdd

// Stack manipulation, common in L-systems
[
]

// Stores key/value as shape attribute
set(string shape_attribute_name, any value)

// Toggles visibility of shape
hide
show

// Repeats given shape op string and increment 'index' shape attribute
repeat(int number_of_repetitions, shape_op_string operations)
// Same as 'repeat' but doesn't push a new shape on the stack for every repetition.
repeatNoPush(int number_of_repetitions, shape_op_string operations)
```

### Splits (Repeat, Subdivision, and Components)

```
// Repeat splits
splitRepeatX(float size, shape_op_string operations)
splitRepeatY(float size, shape_op_string operations)
splitRepeatZ(float size, shape_op_string operations)

// Subdivision splits
// The pattern strings consists of "fs()":
// - f stands for a fixed size element
// - s stands for a stretchable element
// - (...) everything within parens gets repeated for as long as there is space
// Example: splitX("(s)", size, ...) is the same as splitRepeatX(size, ...)
// Note that 'f' elements either get always fully included or omitted, they never get cut or stretched.
// If 's' elements are present, the available space will always get used up completely.
// There are no nested splits.
splitX(string pattern, float size_1, shape_op_string operations_1, ...
       float size_n, shape_op_string operations_n)
splitY(string pattern, float size_1, shape_op_string operations_1, ...
       float size_n, shape_op_string operations_n)
splitZ(string pattern, float size_1, shape_op_string operations_1, ...
       float size_n, shape_op_string operations_n)

// Face (aka component) splits
splitFace(string selector_1, shape_op_string operations_1, ..., string selector_n, shape_op_string operations_n) 
// Possible selectors: "front", "back", "left", "right", "top", "bottom", "all", "horizontal", "vertical"

splitFaceAlongDir(float direction_x, float direction_y, float direction_y, float threshold_angle, shape_op_string operations)  
splitFaceByIndex(int face_index_1, ..., int face_index_n, shape_op_string operations)
```

## Built-In Functions

### Casts

```
bool(bool boolean)
int(int number)
float(float number)
string(string text)
```

### Random numbers

```
rand_int(int min, int max)            // random integer
rand_uniform(float min, float max)    // uniform random number in [min, max)
rand_normal(float mean, float sigma)  // normal distribution random number
```

### Noise and Fractal Brownian Motion

```go
noise(float pos_x, float pos_y, float pos_z)
noise(float pos_x, float pos_y)

fbm(float pos_x, float pos_y, float pos_z, int octaves)
fbm(float pos_x, float pos_y, float pos_z, int octaves, float lacunarity, float gain)
fbm(float pos_x, float pos_y, int octaves)
fbm(float pos_x, float pos_y, int octaves, float lacunarity, float gain)
```

### Maths

```
abs(int|float number)
ceil(float number)
floor(float number)
round(float number)
sign(float number)
fract(float number)

sin(float angle)
cos(float angle)
tan(float angle)
asin(float number)
acos(float number)
atan(float number)
atan2(float number_1, float number_2)

sqrt(float number)
pow(float number, float exponent)
exp(float number)
log(float number)
log10(float number)

max((int|float) number_1, (int|float) number_2)
min((int|float) number_1, (int|float) number_2)
clamp(float min, float max, float x)
step(float edge, float x)
smooth_step(float edge_1, float edge_2, float x)
lerp(float from, float to, float lambda)

pi

// Seed of current derivation
seed

// Query mesh size
// info_names: size_x, size_y, size_z
mesh_info(string file_path, string info_name)

// Check if asset exists
file_exists(string file_path)
```

## Shape Attributes

Shapes have built-in attributes that can be used as named expressions:

### Transformation and Geometry

```
size_x
size_y
size_z
pos_x
pos_y
pos_z
pos_world_x
pos_world_y
pos_world_z
area
```

### Material

```
color_r
color_g
color_b
color_a
color_specular_r
color_specular_g
color_specular_b
material_name
metallic
roughness
reflectance
texture
```

### Material

```
// Index from split and repeat operations, -1 if none
index

visible

// Depth in shape tree
depth

// Name of rule that created shape
label

// Read custom shape attribute set with 'get' shape op
get(string custom_shape_attribute_name)

// Occlusion query against all shapes in octree
occlusion                            // Possible values: full, partial, none
// Occlusion query against specific type of shapes in octree
occlusion(string other_shape_label)  // possible values: full, partial, none
```
