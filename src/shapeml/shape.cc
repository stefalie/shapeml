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

#include "shapeml/shape.h"

#include <cassert>
#include <memory>

#include "shapeml/evaluator.h"
#include "shapeml/interpreter.h"

namespace shapeml {

Shape::Shape() {
  // Reset the FFD cage if it has not been setup yet.
  if (ffd_.cps.empty()) {
    FFDSetCageAndReset(Vec3i(1, 1, 1));
  }
}

Shape::~Shape() {
  for (Shape* c : children_) {
    delete c;
  }
}

Shape* Shape::CreateOffspring() {
  Shape* ret = new Shape(*this);
  ret->world_trafo_dirty_ = true;
  ret->scope_.trafo = Isometry3::Identity();
  ret->children_.clear();
  ret->parent_ = this;
  ret->rule_.reset();
  ret->has_texture_projection_ = false;
  ret->trim_planes_.clear();
  return ret;
}

void Shape::SetMeshIntoScope(geometry::HalfedgeMeshConstPtr mesh) {
  mesh_ = mesh;

  geometry::AABB aabb;
  mesh_->FillAABB(&aabb);
  const Vec3 dim = aabb.extent * 2.0;
  Vec3 scale = Size().cwiseQuotient(dim);
  for (size_t i = 0; i < 3; ++i) {
    if (dim[i] == 0.0) {
      scale[i] = 1.0;
    }
  }

  if (SizeX() == 0.0 && SizeY() == 0.0 && SizeZ() == 0.0) {
    Size(dim);
  } else if (SizeX() == 0.0 && SizeY() == 0.0) {
    Size(dim * scale.z());
  } else if (SizeX() == 0.0 && SizeZ() == 0.0) {
    Size(dim * scale.y());
  } else if (SizeY() == 0.0 && SizeZ() == 0.0) {
    Size(dim * scale.x());
  } else if (SizeX() == 0.0) {
    SizeX(dim.x() * (scale.y() + scale.z()) * 0.5);
  } else if (SizeY() == 0.0) {
    SizeY(dim.y() * (scale.x() + scale.z()) * 0.5);
  } else if (SizeZ() == 0.0) {
    SizeZ(dim.z() * (scale.x() + scale.y()) * 0.5);
  }
}

void Shape::SetMeshAndAdaptScope(geometry::HalfedgeMeshConstPtr mesh) {
  mesh_ = mesh;
  geometry::AABB aabb;
  mesh_->FillAABB(&aabb);

  Translate(aabb.center - aabb.extent);
  Vec3 size = aabb.extent * 2.0;

  // This is probably not necessary.
  for (int i = 0; i < 3; ++i) {
    if (size[i] < geometry::EPSILON) {
      size[i] = 0.0;
    }
  }
  Size(size);
}

void Shape::SetMeshAndAdaptScopeWithRotation(
    geometry::HalfedgeMeshConstPtr mesh, const Mat3& rotation) {
  mesh_ = mesh;
  geometry::AABB aabb;
  mesh_->FillAABB(&aabb);

  scope_.trafo.linear() *= rotation;
  world_trafo_dirty_ = true;

  Translate(aabb.center - aabb.extent);
  Vec3 size = aabb.extent * 2.0;

  // This is probably not necessary.
  for (int i = 0; i < 3; ++i) {
    if (size[i] < geometry::EPSILON) {
      size[i] = 0.0;
    }
  }
  Size(size);
}

void Shape::SetMeshAfterFaceSplit(geometry::HalfedgeMeshPtr mesh,
                                  const Vec3& normal) {
  // Compute the scope frame for this new face component.
  // Nothing fancy, just a silly heuristic.
  const Scalar angle_thres = cos((90.0 - 78.75) / 180.0 * M_PI);
  const Scalar dot = normal.dot(Vec3::UnitY());
  Vec3 y_axis;
  if (dot > angle_thres) {  // Horizontal scope facing up
    y_axis = -Vec3::UnitZ();
  } else if (dot < -angle_thres) {  // Horizontal scope facing down
    y_axis = Vec3::UnitZ();
  } else {  // Vertical scope
    y_axis = Vec3::UnitY();
  }

  Vec3 x_axis = y_axis.cross(normal).normalized();
  y_axis = normal.cross(x_axis).normalized();

  Mat3 rotation;
  rotation.row(0) = x_axis;
  rotation.row(1) = y_axis;
  rotation.row(2) = normal;
  Isometry3 trafo = Isometry3::Identity();
  trafo.linear() = rotation;

  mesh->Transform(trafo, Vec3::Ones());
  SetMeshAndAdaptScopeWithRotation(mesh, rotation.transpose());
}

void Shape::TextureSetupProjectionInXY(Scalar tex_width, Scalar tex_height,
                                       Scalar u_offset, Scalar v_offset) {
  assert(mesh_ && !mesh_->Empty());

  texture_projection_ = Affine3::Identity();
  texture_projection_.scale(Vec3(1.0 / tex_width, 1.0 / tex_height, 0.0));
  texture_projection_.translate(Vec3(u_offset, v_offset, 0.0));
  has_texture_projection_ = true;
}

bool Shape::TextureProjectUV() {
  Affine3 trafo = Affine3::Identity();
  trafo.scale(Size());

  Shape* it = this;
  while (it && !it->has_texture_projection_) {
    trafo = it->scope_.trafo * trafo;
    it = it->parent_;
  }

  if (!it) {
    return true;
  }

  trafo = it->texture_projection_ * trafo;

  geometry::HalfedgeMeshPtr tmp =
      std::make_shared<geometry::HalfedgeMesh>(*mesh_);
  tmp->TexProjectUVInUnitXY(trafo);
  mesh_ = tmp;

  return false;
}

void Shape::Trim(bool local_only) {
  std::vector<geometry::Plane3> local_trim_planes = trim_planes_;

  // Transform parent trim planes into current shapes coordinate system.
  if (!local_only) {
    Isometry3 trafo = scope_.trafo.inverse();

    Shape* it = parent_;
    while (it) {
      for (const geometry::Plane3& p : it->trim_planes_) {
        local_trim_planes.push_back(p.Transform(trafo));
      }

      trafo = trafo * it->scope_.trafo.inverse();
      it = it->parent_;
    }
  }

  geometry::HalfedgeMeshPtr tmp =
      std::make_shared<geometry::HalfedgeMesh>(*mesh_);
  tmp->TransformUnitTrafoAndScale(Size());

  for (const geometry::Plane3& p : local_trim_planes) {
    geometry::HalfedgeMeshPtr below, above;
    tmp->Split(p, &below, &above);

    if (below->Empty()) {
      break;
    }

    tmp.swap(below);
  }

  SetMeshAndAdaptScope(tmp);
}

void Shape::CenterInOtherShape(const Shape* shape, bool x, bool y, bool z) {
  assert(shape);

  // Transform the center of this shape into the coordinate system of the
  // reference shape. Compute the difference vector there.
  const Vec3 center_in_other =
      shape->WorldTrafo().inverse() * (WorldTrafo() * (0.5 * Size()));
  Vec3 diff_other = 0.5 * shape->Size() - center_in_other;

  // Set ignored components to zero.
  diff_other = Vec3(x ? diff_other.x() : 0.0, y ? diff_other.y() : 0.0,
                    z ? diff_other.z() : 0.0);

  // Rotate the difference vector back into the current coordinate system and
  // apply it.
  const Vec3 diff = WorldTrafo().rotation().inverse() *
                    shape->WorldTrafo().rotation() * diff_other;
  Translate(diff);
}

void Shape::RotateScope(const Vec3& axis, Scalar angle) {
  assert(mesh_ && !mesh_->Empty());

  Eigen::AngleAxisd rotation =
      Eigen::AngleAxisd(M_PI / 180.0 * angle, axis.normalized());
  geometry::HalfedgeMeshPtr tmp =
      std::make_shared<geometry::HalfedgeMesh>(*mesh_);

  Isometry3 translation;
  Vec3 combined_scale;
  tmp->GetScaledUnitTrafo(Size(), &translation, &combined_scale);

  tmp->Transform(rotation.inverse() * translation, combined_scale);
  SetMeshAndAdaptScopeWithRotation(tmp, rotation.matrix());
}

void Shape::RotateScopeXYToXZ() {
  TranslateY(SizeY());
  RotateX(90.0);
  const Scalar size_z = SizeZ();
  SizeZ(SizeY());
  SizeY(size_z);

  if (mesh_ && !mesh_->Empty()) {
    Isometry3 rotation = Isometry3::Identity();
    rotation.rotate(Eigen::AngleAxisd(-M_PI * 0.5, Vec3::UnitX()));
    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*mesh_);
    tmp->Transform(rotation, Vec3::Ones());
    SetMeshIntoScope(tmp);
  }
}

Vec3 Shape::Position() const { return scope_.trafo.translation(); }

Scalar Shape::PositionX() const { return scope_.trafo.translation().x(); }

Scalar Shape::PositionY() const { return scope_.trafo.translation().y(); }

Scalar Shape::PositionZ() const { return scope_.trafo.translation().z(); }

Quaternion Shape::RotationAsQuaternion() const {
  return Quaternion(scope_.trafo.rotation());
}

const Mat3 Shape::RotationAsMatrix() const { return scope_.trafo.rotation(); }

const Vec3& Shape::Size() const { return scope_.size; }

Scalar Shape::SizeX() const { return scope_.size.x(); }

Scalar Shape::SizeY() const { return scope_.size.y(); }

Scalar Shape::SizeZ() const { return scope_.size.z(); }

void Shape::Translate(const Vec3& t) {
  scope_.trafo *= Eigen::Translation3d(t);
  world_trafo_dirty_ = true;
}

void Shape::TranslateX(Scalar x) {
  scope_.trafo *= Eigen::Translation3d(Vec3(x, 0.0, 0.0));
  world_trafo_dirty_ = true;
}

void Shape::TranslateY(Scalar y) {
  scope_.trafo *= Eigen::Translation3d(Vec3(0.0, y, 0.0));
  world_trafo_dirty_ = true;
}

void Shape::TranslateZ(Scalar z) {
  scope_.trafo *= Eigen::Translation3d(Vec3(0.0, 0.0, z));
  world_trafo_dirty_ = true;
}

void Shape::Rotate(const Vec3& axis, Scalar angle) {
  scope_.trafo *= Eigen::AngleAxisd(M_PI / 180.0 * angle, axis.normalized());
  world_trafo_dirty_ = true;
}

void Shape::RotateX(Scalar x) {
  scope_.trafo *= Eigen::AngleAxisd(M_PI / 180.0 * x, Vec3::UnitX());
  world_trafo_dirty_ = true;
}

void Shape::RotateY(Scalar y) {
  scope_.trafo *= Eigen::AngleAxisd(M_PI / 180.0 * y, Vec3::UnitY());
  world_trafo_dirty_ = true;
}

void Shape::RotateZ(Scalar z) {
  scope_.trafo *= Eigen::AngleAxisd(M_PI / 180.0 * z, Vec3::UnitZ());
  world_trafo_dirty_ = true;
}

void Shape::Size(const Vec3& s) { scope_.size = s; }

void Shape::SizeX(Scalar x) { scope_.size.x() = x; }

void Shape::SizeY(Scalar y) { scope_.size.y() = y; }

void Shape::SizeZ(Scalar z) { scope_.size.z() = z; }

void Shape::Scale(const Vec3& s) { scope_.size = scope_.size.cwiseProduct(s); }

void Shape::ScaleX(Scalar x) { scope_.size.x() *= x; }

void Shape::ScaleY(Scalar y) { scope_.size.y() *= y; }

void Shape::ScaleZ(Scalar z) { scope_.size.z() *= z; }

void Shape::SetRotation(const Mat3& rotation) {
  // Maybe the following would be enough. I think latter one guarantees that our
  // matrix is orthonormal.
  // scope_.trafo.linear() = rotation;
  scope_.trafo.linear() = Eigen::AngleAxisd(rotation).matrix();
  world_trafo_dirty_ = true;
}

void Shape::FFDSetCageAndReset(const Vec3i& subdivs) {
  assert(subdivs.x() > 0 && subdivs.y() > 0 && subdivs.z() > 0);
  ffd_.l = subdivs.x();
  ffd_.m = subdivs.y();
  ffd_.n = subdivs.z();
  ffd_.cps.resize((ffd_.l + 1) * (ffd_.m + 1) * (ffd_.n + 1));

  // Hard coded default CPs for 2x2x2.
  // ffd_.cps[0] = Vec3::Zero();
  // ffd_.cps[1] = Vec3::UnitX();
  // ffd_.cps[2] = Vec3::UnitY();
  // ffd_.cps[3] = Vec3(1.0, 1.0, 0.0);
  // ffd_.cps[4] = Vec3::UnitZ();
  // ffd_.cps[5] = Vec3(1.0, 0.0, 1.0);
  // ffd_.cps[6] = Vec3(0.0, 1.0, 1.0);
  // ffd_.cps[7] = Vec3(1.0, 1.0, 1.0);

  const double step_l = 1.0 / ffd_.l;
  const double step_m = 1.0 / ffd_.m;
  const double step_n = 1.0 / ffd_.n;

  for (unsigned k = 0; k <= ffd_.n; ++k) {
    for (unsigned j = 0; j <= ffd_.m; ++j) {
      for (unsigned i = 0; i <= ffd_.l; ++i) {
        ffd_.cps[i + j * (ffd_.l + 1) + k * (ffd_.l + 1) * (ffd_.m + 1)] =
            Vec3(i * step_l, j * step_m, k * step_n);
      }
    }
  }
}

void Shape::FFDTranslateCP(const Vec3i& cp, const Vec3& t) {
  const unsigned i = cp.x();
  const unsigned j = cp.y();
  const unsigned k = cp.z();
  assert(!(i > ffd_.l || j > ffd_.m || k > ffd_.n));

  const Vec3 t_rel = t.cwiseQuotient(Size());
  ffd_.cps[i + (ffd_.l + 1) * j + (ffd_.l + 1) * (ffd_.m + 1) * k] += t_rel;

  // Version for 2x2x2.
  // ffd_.cps[i | j << 1 | k << 2] += t_rel;
}

void Shape::FFDApply() {
  for (size_t i = 0; i < ffd_.cps.size(); ++i) {
    ffd_.cps[i] = ffd_.cps[i].cwiseProduct(Size());
  }

  geometry::HalfedgeMeshPtr tmp =
      std::make_shared<geometry::HalfedgeMesh>(*mesh_);
  tmp->TransformUnitTrafoAndScale(Vec3::Ones());
  tmp->TransformFFD(ffd_.cps, ffd_.l, ffd_.m, ffd_.n);
  SetMeshAndAdaptScope(tmp);

  FFDSetCageAndReset(Vec3i(ffd_.l, ffd_.m, ffd_.n));
}

void Shape::SetColor(const Vec4& color) { material_.color = color; }

void Shape::SetMetallic(Scalar metallic) { material_.metallic = metallic; }

void Shape::SetRoughness(Scalar roughness) { material_.roughness = roughness; }

void Shape::SetReflectance(Scalar reflectance) {
  material_.reflectance = reflectance;
}

void Shape::SetTextureDiffuse(const std::string& file_name) {
  material_.texture = file_name;
}

void Shape::SetMaterialName(const std::string& name) { material_.name = name; }

bool Shape::SetCustomAttribute(const std::string& name, Value value) {
  auto it = custom_attributes_.find(name);

  // Abort if there is a built-in shape attribute of that name.
  if (ShapeAttributeEvaluator::HasEvaluator(name)) {
    return true;
  }

  // Erase an old value in case we're overwriting an attribute.
  if (it != custom_attributes_.end()) {
    custom_attributes_.erase(it);
  }
  custom_attributes_.emplace(name, value);
  return false;
}

bool Shape::GetCustomAttribute(const std::string& name, Value* value) const {
  auto it = custom_attributes_.find(name);
  if (it == custom_attributes_.end()) {
    return false;
  }
  *value = it->second;
  return true;
}

bool Shape::GetParameter(const std::string& name, Value* value) const {
  assert(rule_);
  const ArgVec& args = rule_->arguments;
  for (size_t i = 0; i < args.size(); ++i) {
    if (name == args[i]) {
      *value = parameters_[i];
      return true;
    }
  }
  return false;
}

void Shape::AppendToParent() {
  assert(parent_);
  parent_->children_.push_back(this);
}

bool Shape::IsAnchestor(const Shape* shape) const {
  const Shape* s = this;
  while (s) {
    if (s == shape) {
      return true;
    }
    s = s->parent_;
  }
  return false;
}

int Shape::Depth() const {
  int depth = 0;
  const Shape* shape = this;
  while ((shape = shape->parent_)) {
    ++depth;
  }
  return depth;
}

const Isometry3& Shape::WorldTrafo() const {
  if (world_trafo_dirty_) {
    if (parent_) {
      world_trafo_ = parent_->WorldTrafo() * scope_.trafo;
    } else {
      world_trafo_ = scope_.trafo;
    }
    world_trafo_dirty_ = false;
  }
  return world_trafo_;
}

Affine3 Shape::MeshWorldTrafo() const {
  assert(mesh_);
  Isometry3 translation;
  Vec3 combined_scale;
  mesh_->GetScaledUnitTrafo(Size(), &translation, &combined_scale);
  return WorldTrafo() * translation * Eigen::Scaling(combined_scale);
}

Mat3 Shape::MeshNormalMatrix() const {
  assert(mesh_);
  const Vec3 combined_scale = Size().cwiseProduct(mesh_->UnitTrafoScale());
  return WorldTrafo().linear() * mesh_->NormalMatrixFromScale(combined_scale);
}

geometry::AABB Shape::GetWorldSpaceAABB() const {
  const Isometry3& trafo = WorldTrafo();
  const Vec3& size = scope_.size;
  Vec3Vec vertices;
  vertices.push_back(trafo * Vec3(0.0, 0.0, 0.0));
  vertices.push_back(trafo * Vec3(size.x(), 0.0, 0.0));
  vertices.push_back(trafo * Vec3(0.0, size.y(), 0.0));
  vertices.push_back(trafo * Vec3(size.x(), size.y(), 0.0));
  vertices.push_back(trafo * Vec3(0.0, 0.0, size.z()));
  vertices.push_back(trafo * Vec3(size.x(), 0.0, size.z()));
  vertices.push_back(trafo * Vec3(0.0, size.y(), size.z()));
  vertices.push_back(trafo * Vec3(size.x(), size.y(), size.z()));
  return geometry::AABB(vertices);
}

geometry::OBB Shape::GetWorldSpaceOBB() const {
  geometry::OBB obb;
  const Isometry3& trafo = WorldTrafo();
  const Mat3& rot = trafo.linear();
  obb.axes[0] = rot.col(0);
  obb.axes[1] = rot.col(1);
  obb.axes[2] = rot.col(2);
  obb.extent = scope_.size * 0.5;
  obb.center = trafo.translation() + rot * obb.extent;
  return obb;
}

void Shape::AcceptVisitor(ShapeConstVisitor* visitor) const {
  visitor->Visit(this);
  for (const Shape* c : children_) {
    c->AcceptVisitor(visitor);
  }
}

void Shape::AcceptVisitor(ShapeVisitor* visitor) {
  visitor->Visit(this);
  for (Shape* c : children_) {
    c->AcceptVisitor(visitor);
  }
}

void Shape::PrintToStreamRecursively(std::ostream* stream) const {
  struct PrintVisitor : public ShapeConstVisitor {
    explicit PrintVisitor(std::ostream* s) : stream(s) {}

    void Visit(const Shape* shape) final {
      for (int i = 0; i < shape->Depth(); ++i) {
        *stream << "  ";
      }
      *stream << *shape << '\n';
    }

    std::ostream* stream;
  } visitor(stream);
  this->AcceptVisitor(&visitor);
}

static inline std::ostream& operator<<(std::ostream& stream, const Vec3& vec) {
  stream << "(" << vec.x() << ", " << vec.y() << ", " << vec.z() << ")";
  return stream;
}

static inline std::ostream& operator<<(std::ostream& stream, const Vec4& vec) {
  stream << "(" << vec.x() << ", " << vec.y() << ", " << vec.z() << ", "
         << vec.w() << ")";
  return stream;
}

// TODO(stefalie): Note that this is not quite complete, there several shape
// properties that are not printed. They should probably be added.
std::ostream& operator<<(std::ostream& stream, const Shape& shape) {
  stream << "name: \"" << shape.name() << '"';
  if (shape.index() >= 0) {
    stream << "; index: " << shape.index();
  }

  // Scope
  stream << "; scope position: " << shape.Position();
  const Quaternion rot = shape.RotationAsQuaternion();
  stream << "; scope rotation: " << Vec3(rot.vec()) << ", " << rot.w();
  stream << "; scope size: " << shape.Size();

  // Other properties
  stream << "; mesh: " << (shape.mesh() ? "yes" : "no");
  stream << "; visible: " << (shape.visible() ? "yes" : "no");
  stream << "; terminal: " << (shape.terminal() ? "yes" : "no");

  // Material
  if (!shape.material().name.empty()) {
    stream << "; material name: \"" << shape.material().name << '"';
  }
  stream << "; material color: " << shape.material().color;
  stream << "; material metallic: " << shape.material().metallic;
  stream << "; material roughness: " << shape.material().roughness;
  if (!shape.material().texture.empty()) {
    stream << "; material texture: \"" << shape.material().texture << '"';
  }

  stream << '.';
  return stream;
}

OcclusionShape::OcclusionShape(const Shape* shape)
    : geometry::OctreeElement(shape->GetWorldSpaceAABB()),
      name_(shape->name()),
      parent_(shape->parent()) {
  obb_ = shape->GetWorldSpaceOBB();
}

void AABBVisitor::Reset() { initialized_ = false; }

void AABBVisitor::Visit(const Shape* shape) {
  if (initialized_) {
    aabb_.Merge(shape->GetWorldSpaceAABB());
  } else {
    aabb_ = shape->GetWorldSpaceAABB();
    initialized_ = true;
  }
}

void LeafConstVisitor::Visit(const Shape* shape) {
  if (shape->IsLeaf()) {
    leaves_.push_back(shape);
  }
}

void LeafVisitor::Visit(Shape* shape) {
  if (shape->IsLeaf()) {
    leaves_.push_back(shape);
  }
}

}  // namespace shapeml
