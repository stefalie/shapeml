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

#include <ostream>
#include <string>
#include <vector>

#include "shapeml/expressions.h"
#include "shapeml/geometry/halfedge_mesh.h"
#include "shapeml/geometry/octree.h"
#include "shapeml/material.h"

namespace shapeml {

class Shape;
typedef std::vector<Shape*> ShapePtrVec;

struct ShapeConstVisitor {
  virtual void Visit(const Shape* shape) = 0;
};

struct ShapeVisitor {
  virtual void Visit(Shape* shape) = 0;
};

struct Scope {
  Isometry3 trafo = Isometry3::Identity();
  Vec3 size = Vec3::Ones();
};

class Shape {
 public:
  Shape();
  ~Shape();

  // Returns an offspring shape. You are responsible for managing the allocated
  // memory.
  Shape* CreateOffspring();

  void SetMeshIntoScope(geometry::HalfedgeMeshConstPtr mesh);
  void SetMeshAndAdaptScope(geometry::HalfedgeMeshConstPtr mesh);
  void SetMeshAndAdaptScopeWithRotation(geometry::HalfedgeMeshConstPtr mesh,
                                        const Mat3& rotation);

  // Careful, this one will alter the mesh.
  void SetMeshAfterFaceSplit(geometry::HalfedgeMeshPtr mesh,
                             const Vec3& normal);

  void TextureSetupProjectionInXY(Scalar tex_width, Scalar tex_height,
                                  Scalar u_offset, Scalar v_offset);
  void TextureSetupProjectionInXY(Scalar tex_width, Scalar tex_height) {
    TextureSetupProjectionInXY(tex_width, tex_height, 0.0, 0.0);
  }
  bool TextureProjectUV();

  void AddTrimPlane(const geometry::Plane3& plane) {
    trim_planes_.push_back(plane);
  }
  void Trim(bool local_only);

  void CenterInOtherShape(const Shape* shape, bool x, bool y, bool z);

  void RotateScope(const Vec3& axis, Scalar angle);
  void RotateScopeXYToXZ();  // Special case of RotateScope

  // Transformation getters
  Vec3 Position() const;
  Scalar PositionX() const;
  Scalar PositionY() const;
  Scalar PositionZ() const;
  Quaternion RotationAsQuaternion() const;
  const Mat3 RotationAsMatrix() const;
  const Vec3& Size() const;
  Scalar SizeX() const;
  Scalar SizeY() const;
  Scalar SizeZ() const;

  // Transformations. Translations and rotations stack similar to the
  // traditional OpenGL transformation stack. The scaling and size functions
  // don't stack. Scaling functions are relative, size function set the absolute
  // size.
  void Translate(const Vec3& t);
  void TranslateX(Scalar x);
  void TranslateY(Scalar y);
  void TranslateZ(Scalar z);
  void Rotate(const Vec3& axis, Scalar angle);
  void RotateX(Scalar x);
  void RotateY(Scalar y);
  void RotateZ(Scalar z);
  void Size(const Vec3& s);
  void SizeX(Scalar x);
  void SizeY(Scalar y);
  void SizeZ(Scalar z);
  void Scale(const Vec3& s);
  void ScaleX(Scalar x);
  void ScaleY(Scalar y);
  void ScaleZ(Scalar z);

  void SetRotation(const Mat3& rotation);

  // Free Form Deformation (FFD)
  // Based on: Sederberg, 1986: Free-Form Deformation of Solid Geometric Models
  void FFDSetCageAndReset(const Vec3i& subdivs);
  void FFDTranslateCP(const Vec3i& cp, const Vec3& t);
  void FFDApply();
  Vec3i FFDResolution() const { return Vec3i(ffd_.l, ffd_.m, ffd_.n); }

  void SetColor(const Vec4& color);
  void SetMetallic(Scalar metallic);
  void SetRoughness(Scalar roughness);
  void SetReflectance(Scalar reflectance);
  void SetTextureDiffuse(const std::string& file_name);
  void SetMaterialName(const std::string& name);

  bool SetCustomAttribute(const std::string& name, Value value);

  // Returns true if the requested custom attribute exists.
  bool GetCustomAttribute(const std::string& name, Value* value) const;

  // Returns true if the requested parameter exists.
  bool GetParameter(const std::string& name, Value* value) const;

  // Adds the shape to the shape tree by adding it to the parent's children.
  // WARNING: Don't change the shape anymore after finalizing it (or at least
  // not once the shape has children). When a shape changes, the changes won't
  // be propagated to the children anymore. Once it's in the tree, it's final.
  // It's a design decision. Easier to code this way and also more performant.
  void AppendToParent();

  bool IsLeaf() const { return children_.empty(); }
  bool IsAnchestor(const Shape* shape) const;

  int Depth() const;

  const Isometry3& WorldTrafo() const;

  Affine3 MeshWorldTrafo() const;
  Mat3 MeshNormalMatrix() const;

  geometry::AABB GetWorldSpaceAABB() const;
  geometry::OBB GetWorldSpaceOBB() const;

  // Visiting
  void AcceptVisitor(ShapeConstVisitor* visitor) const;
  void AcceptVisitor(ShapeVisitor* visitor);

  // Setters and getters.
  void set_name(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  void set_mesh(geometry::HalfedgeMeshConstPtr mesh) { mesh_ = mesh; }
  geometry::HalfedgeMeshConstPtr mesh() const { return mesh_; }

  const Scope& scope() const { return scope_; }

  const ShapePtrVec& children() const { return children_; }
  const Shape* parent() const { return parent_; }

  const Material& material() const { return material_; }

  void set_index(int index) { index_ = index; }
  int index() const { return index_; }

  void set_parameters(const ValueVec& parameters) { parameters_ = parameters; }
  const ValueVec& parameters() const { return parameters_; }

  void set_rule(RuleConstPtr rule) { rule_ = rule; }
  RuleConstPtr rule() const { return rule_; }

  void set_visible(bool visible) { visible_ = visible; }
  bool visible() const { return visible_; }

  void set_terminal(bool terminal) { terminal_ = terminal; }
  bool terminal() const { return terminal_; }

  void PrintToStreamRecursively(std::ostream* stream) const;

 private:
  std::string name_;

  geometry::HalfedgeMeshConstPtr mesh_;

  mutable Isometry3 world_trafo_;
  mutable bool world_trafo_dirty_ = true;

  Scope scope_;

  struct FFD {
    unsigned l = 1;
    unsigned m = 1;
    unsigned n = 1;
    Vec3Vec cps;
  } ffd_;

  ShapePtrVec children_;
  Shape* parent_ = nullptr;

  Material material_;

  int index_ = -1;

  ValueDict custom_attributes_;
  ValueVec parameters_;

  RuleConstPtr rule_;

  bool visible_ = true;

  bool terminal_ = false;

  bool has_texture_projection_ = false;
  Affine3 texture_projection_;

  std::vector<geometry::Plane3> trim_planes_;
};

std::ostream& operator<<(std::ostream& stream, const Shape& shape);

typedef std::vector<Shape*> ShapePtrVec;
typedef std::vector<const Shape*> ShapeConstPtrVec;

class OcclusionShape : public geometry::OctreeElement {
 public:
  explicit OcclusionShape(const Shape* shape);

  const std::string& name() const { return name_; }
  const Shape* parent() const { return parent_; }
  const geometry::OBB& obb() const { return obb_; }

 private:
  const std::string name_;
  const Shape* parent_;
  geometry::OBB obb_;
};

class AABBVisitor : public ShapeConstVisitor {
 public:
  void Reset();
  void Visit(const Shape* shape) final;
  const geometry::AABB& aabb() const { return aabb_; }

 private:
  geometry::AABB aabb_;
  bool initialized_ = false;
};

class LeafConstVisitor : public ShapeConstVisitor {
 public:
  void Visit(const Shape* shape) final;
  const ShapeConstPtrVec& leaves() const { return leaves_; }

 private:
  ShapeConstPtrVec leaves_;
};

class LeafVisitor : public ShapeVisitor {
 public:
  void Visit(Shape* shape) final;
  const ShapePtrVec& leaves() const { return leaves_; }

 private:
  ShapePtrVec leaves_;
};

}  // namespace shapeml
