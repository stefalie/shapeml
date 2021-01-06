// Shape Modeling Language (ShapeML)
// Copyright (C) 2019 Stefan Lienhard
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <surface_mesh/Surface_mesh.h>

#include <memory>
#include <string>
#include <vector>

#include "shapeml/geometry/vector_types.h"

namespace shapeml {

namespace geometry {

struct AABB;
struct Plane3;

class HalfedgeMesh;
typedef std::shared_ptr<HalfedgeMesh> HalfedgeMeshPtr;
typedef std::shared_ptr<const HalfedgeMesh> HalfedgeMeshConstPtr;

struct RenderVertex {
  float position[3];
  float normal[3];
  float uv[2];
};

class HalfedgeMesh {
 public:
  HalfedgeMesh();

  void FromPolygon(const Vec3Vec& polygon, const Vec3Vec* normals,
                   const Vec2Vec* uvs);
  void FromIndexedVertices(const Vec3Vec& vertices,
                           const IdxVecVec& vertex_indices,
                           const Vec3Vec* normals,
                           const IdxVecVec* normal_indices, const Vec2Vec* uvs,
                           const IdxVecVec* uv_indices,
                           const std::string* file_name);
  bool FromFile(const std::string& file_name);

  int NumFaces() const { return mesh_.n_faces(); }

  void TexProjectUVInUnitXY(const Affine3& tex_trafo);
  void TexTransformUV(const Affine2& tex_trafo);

  void FlipWinding(bool flip_normals);

  void Mirror(const Plane3& mirror_plane);

  // Returns the area of the face with index idx.
  Scalar GetFaceArea(unsigned idx) const;

  // Extrudes a single polygon and returns false, or true if an error occurred.
  bool ExtrudeAlongNormal(Scalar length);
  bool ExtrudeAlongDirection(const Vec3& direction, Scalar length);

  // Roofs
  void ExtrudeRoofHipOrGable(Scalar angle, Scalar overhang_side, bool gable,
                             Scalar overhang_gable);
  void ExtrudeRoofPyramid(Scalar height, Scalar overhang_height);
  void ExtrudeRoofShed(Scalar angle);

  void Split(const Plane3& plane, HalfedgeMeshPtr* below,
             HalfedgeMeshPtr* above) const;

  // These two methods are required and sufficient for implementing component
  // splits for faces.
  const Vec3& GetFaceNormal(int face_idx) const;
  HalfedgeMeshPtr GetFaceComponent(int face_idx) const;

  // Creates an interleaved render buffer.
  void FillRenderBuffer(std::vector<RenderVertex>* vertex_data,
                        IdxVec* indices) const;

  // Fills buffers for file exporters.
  // TODO(stefalie): Add a booelan parameter 'triangulize' that allows toggling
  // the output between triangles and polygons of aribtrary size. Note that non-
  // triangulized geometry would also require an array of polygon sizes.
  void FillExportBuffers(Vec3Vec* vertices, Vec3Vec* normals, Vec2Vec* uvs,
                         IdxVec* vertex_indices, IdxVec* normal_indices,
                         IdxVec* uv_indices) const;

  void FillAABB(AABB* aabb) const;

  void Transform(const Isometry3& trafo, const Vec3& scale);
  void TransformUnitTrafoAndScale(const Vec3& scale);
  void TransformFFD(const Vec3Vec& cps, unsigned l, unsigned m, unsigned n);

  // Make it flat shaded by only keeping face normals.
  void RemoveFaceVertexNormals() { has_face_vertex_normals_ = false; }

  // Make it smooth shaded by computing face vertex normals as weighted averages
  // over all faces. The weights are the face areas.
  void ComputeFaceVertexNormals();

  bool Empty() const { return static_cast<bool>(mesh_.empty()); }

  // The unit transformation changes the mesh so that it is contained an AABB
  // that reaches from (0, 0, 0) to (1, 1, 1). Degenerate dimensions will be
  // reduced to 0.
  const Vec3& UnitTrafoTranslation() const;
  const Vec3& UnitTrafoScale() const;

  void GetScaledUnitTrafo(const Vec3& scale, Isometry3* translation,
                          Vec3* combined_scale) const;

  Mat3 NormalMatrixFromScale(const Vec3& scale) const;

  bool has_uv0() const { return has_uv0_; }

 private:
  void UpdateUnitTrafo() const;

  surface_mesh::Surface_mesh mesh_;

  // This will get set to true if you load a mesh from a file that has normals
  // per face vertex. It will get set to false when extruded. For splitting and
  // component splitting the face vertex normals will get handled/interpolated
  // correctly.
  bool has_face_vertex_normals_;

  bool has_uv0_;  // Called 0 because we hope to support multi-texturing soon.

  // The unit transformation transforms the mesh into the unit cube from
  // (0, 0, 0) to (1, 1, 1). Degenerate dimensions will be reduced to 0. It
  // consists only of translation and scaling.
  mutable Vec3 unit_trafo_translation_;
  mutable Vec3 unit_trafo_scale_;
  mutable bool unit_trafo_dirty_ = true;
};

struct Vec3iHash {
  std::size_t operator()(const Vec3i& vi) const;
};

struct Vec2iHash {
  std::size_t operator()(const Vec2i& vi) const;
};

}  // namespace geometry

}  // namespace shapeml
