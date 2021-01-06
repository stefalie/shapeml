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

#include "shapeml/geometry/halfedge_mesh.h"

#include <algorithm>
#include <set>
#include <unordered_map>

#include "shapeml/geometry/obj_loader.h"
#include "shapeml/geometry/ray_plane_bv.h"
#include "shapeml/geometry/skeleton/roof.h"
#include "shapeml/geometry/skeleton/skeleton.h"
#include "shapeml/geometry/triangulator.h"

namespace shapeml {

namespace geometry {

namespace {

typedef surface_mesh::Surface_mesh SurfaceMesh;
typedef SurfaceMesh::Vertex Vertex;
typedef SurfaceMesh::Edge Edge;
typedef SurfaceMesh::Halfedge Halfedge;
typedef SurfaceMesh::Face Face;
typedef SurfaceMesh::Vertex_iterator Vertex_iterator;
typedef SurfaceMesh::Edge_iterator Edge_iterator;
typedef SurfaceMesh::Halfedge_iterator Halfedge_iterator;
typedef SurfaceMesh::Face_iterator Face_iterator;
typedef SurfaceMesh::Halfedge_around_vertex_circulator
    Halfedge_around_vertex_circulator;
typedef SurfaceMesh::Face_around_vertex_circulator
    Face_around_vertex_circulator;
typedef SurfaceMesh::Vertex_around_face_circulator
    Vertex_around_face_circulator;
typedef SurfaceMesh::Halfedge_around_face_circulator
    Halfedge_around_face_circulator;
template <class T>
using Vertex_property = SurfaceMesh::Vertex_property<T>;
template <class T>
using Edge_property = SurfaceMesh::Edge_property<T>;
template <class T>
using Halfedge_property = SurfaceMesh::Halfedge_property<T>;
template <class T>
using Face_property = SurfaceMesh::Face_property<T>;

}  // namespace

HalfedgeMesh::HalfedgeMesh() {
  mesh_.add_face_property<Vec3>("f:normal");
  mesh_.add_halfedge_property<Vec3>("h:normal");
  mesh_.add_halfedge_property<Vec2>("h:uv0");
}

void HalfedgeMesh::FromPolygon(const Vec3Vec& polygon, const Vec3Vec* normals,
                               const Vec2Vec* uvs) {
  assert(polygon.size() > 2);
  mesh_.clear();
  std::vector<Vertex> vertices;
  for (const Vec3& v : polygon) {
    vertices.push_back(mesh_.add_vertex(v));
  }
  Face face = mesh_.add_face(vertices);
  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");
  f_normals[face] = mesh_.compute_face_normal(face);

  if (normals) {
    Halfedge_property<Vec3> fv_normals =
        mesh_.get_halfedge_property<Vec3>("h:normal");
    assert(vertices.size() == normals->size());

    Halfedge_around_face_circulator hf_it, hf_it_end;
    hf_it = hf_it_end = mesh_.halfedges(face);
    unsigned v_idx = 0;
    do {
      fv_normals[*hf_it] = (*normals)[v_idx++];
    } while (++hf_it != hf_it_end);

    has_face_vertex_normals_ = true;
  } else {
    has_face_vertex_normals_ = false;
  }

  if (uvs) {
    Halfedge_property<Vec2> uv0s = mesh_.get_halfedge_property<Vec2>("h:uv0");
    assert(vertices.size() == uvs->size());

    Halfedge_around_face_circulator hf_it, hf_it_end;
    hf_it = hf_it_end = mesh_.halfedges(face);
    unsigned v_idx = 0;
    do {
      uv0s[*hf_it] = (*uvs)[v_idx++];
    } while (++hf_it != hf_it_end);

    has_uv0_ = true;
  } else {
    has_uv0_ = false;
  }

  unit_trafo_dirty_ = true;
}

void HalfedgeMesh::FromIndexedVertices(
    const Vec3Vec& vertices, const IdxVecVec& vertex_indices,
    const Vec3Vec* normals, const IdxVecVec* normal_indices, const Vec2Vec* uvs,
    const IdxVecVec* uv_indices, const std::string* file_name) {
  mesh_.clear();
  for (const Vec3& v : vertices) {
    mesh_.add_vertex(v);
  }

  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");
  Halfedge_property<Vec3> fv_normals =
      mesh_.get_halfedge_property<Vec3>("h:normal");
  Halfedge_property<Vec2> uv0s = mesh_.get_halfedge_property<Vec2>("h:uv0");
  for (unsigned i = 0; i < vertex_indices.size(); ++i) {
    assert(vertex_indices[i].size() > 2);

    std::vector<Vertex> face_verts;
    for (unsigned j = 0; j < vertex_indices[i].size(); ++j) {
      const Vertex vertex = Vertex(vertex_indices[i][j]);
      face_verts.push_back(vertex);
    }
    Face face = mesh_.add_face(face_verts);
    if (face.idx() < 0) {
      assert(file_name);
      std::cerr << "WARNING (file: " << *file_name << "): ";
      std::cerr << "Ignoring face with complex edge.\n";
      continue;
    }
    f_normals[face] = mesh_.compute_face_normal(face);

    if (normals && normal_indices) {
      assert(vertex_indices.size() == normal_indices->size());
      assert(vertex_indices[i].size() == (*normal_indices)[i].size());

      Halfedge_around_face_circulator hf_it, hf_it_end;
      hf_it = hf_it_end = mesh_.halfedges(face);
      unsigned v_idx = 0;
      do {
        fv_normals[*hf_it] = (*normals)[(*normal_indices)[i][v_idx++]];
      } while (++hf_it != hf_it_end);
    }

    if (uvs && uv_indices) {
      assert(vertex_indices.size() == uv_indices->size());
      assert(vertex_indices[i].size() == (*uv_indices)[i].size());

      Halfedge_around_face_circulator hf_it, hf_it_end;
      hf_it = hf_it_end = mesh_.halfedges(face);
      unsigned v_idx = 0;
      do {
        uv0s[*hf_it] = (*uvs)[(*uv_indices)[i][v_idx]];
        ++v_idx;
      } while (++hf_it != hf_it_end);
    }
  }

  if (normals && normal_indices) {
    has_face_vertex_normals_ = true;
  } else {
    has_face_vertex_normals_ = false;
  }

  if (uvs && uv_indices) {
    has_uv0_ = true;
  } else {
    has_uv0_ = false;
  }

  mesh_.garbage_collection();  // Maybe not necessary.
  unit_trafo_dirty_ = true;
}

// This method assumes that every face vertex has a corresponding normal index
// if the file contains any normals. The same is true for texture coordinates.
// If this is violated, e.g., by having some faces that are annotated with
// normal/tex_coord indices and other that are not, the indices that were not
// provided will be undefined.
bool HalfedgeMesh::FromFile(const std::string& file_name) {
  OBJLoader obj_loader;
  OBJMesh* mesh = obj_loader.Load(file_name, false);
  if (!mesh) {
    return true;
  }

  Vec3Vec vertices;
  for (unsigned i = 0; i < mesh->vertices.size(); i += 3) {
    vertices.push_back(Vec3(mesh->vertices[i + 0], mesh->vertices[i + 1],
                            mesh->vertices[i + 2]));
  }
  IdxVecVec vertex_indices;
  for (unsigned i = 0; i < mesh->face_sets.size(); ++i) {
    for (unsigned j = 0; j < mesh->face_sets[i].size(); ++j) {
      vertex_indices.push_back(IdxVec());
      for (unsigned k = 0; k < mesh->face_sets[i][j].size(); ++k) {
        vertex_indices.back().push_back(mesh->face_sets[i][j][k].vertex_index);
      }
    }
  }

  Vec3Vec* normals_ptr = nullptr;
  IdxVecVec* normal_indices_ptr = nullptr;
  Vec3Vec normals;
  IdxVecVec normal_indices;
  if (mesh->normals.size() > 0) {
    for (unsigned i = 0; i < mesh->normals.size(); i += 3) {
      normals.push_back(Vec3(mesh->normals[i + 0], mesh->normals[i + 1],
                             mesh->normals[i + 2]));
    }
    for (unsigned i = 0; i < mesh->face_sets.size(); ++i) {
      for (unsigned j = 0; j < mesh->face_sets[i].size(); ++j) {
        normal_indices.push_back(IdxVec());
        for (unsigned k = 0; k < mesh->face_sets[i][j].size(); ++k) {
          normal_indices.back().push_back(
              mesh->face_sets[i][j][k].normal_index);
        }
      }
    }
    normals_ptr = &normals;
    normal_indices_ptr = &normal_indices;
  }

  Vec2Vec* uvs_ptr = nullptr;
  IdxVecVec* uv_indices_ptr = nullptr;
  Vec2Vec uvs;
  IdxVecVec uv_indices;
  if (mesh->tex_coords.size() > 0) {
    for (unsigned i = 0; i < mesh->tex_coords.size(); i += 2) {
      uvs.push_back(Vec2(mesh->tex_coords[i + 0], mesh->tex_coords[i + 1]));
    }
    for (unsigned i = 0; i < mesh->face_sets.size(); ++i) {
      for (unsigned j = 0; j < mesh->face_sets[i].size(); ++j) {
        uv_indices.push_back(IdxVec());
        for (unsigned k = 0; k < mesh->face_sets[i][j].size(); ++k) {
          uv_indices.back().push_back(mesh->face_sets[i][j][k].tex_coord_index);
        }
      }
    }
    uvs_ptr = &uvs;
    uv_indices_ptr = &uv_indices;
  }

  if (normal_indices_ptr) {
    if (normal_indices_ptr->size() != vertex_indices.size()) {
      std::cerr << "WARNING (file: " << file_name << "): ";
      std::cerr << "Some faces have normal indices, others don't. ";
      std::cerr << "This results in ignoring all normal indices.\n";
      normal_indices_ptr = nullptr;
    } else {
      for (unsigned i = 0; i < normal_indices_ptr->size(); ++i) {
        if ((*normal_indices_ptr)[i].size() != vertex_indices[i].size()) {
          std::cerr << "WARNING (file: " << file_name << "): ";
          std::cerr
              << "A face has normal indices for some but not all vertices. ";
          std::cerr << "This results in ignoring all normal indices.\n";
          normal_indices_ptr = nullptr;
          break;
        }
      }
    }
  }

  if (uv_indices_ptr) {
    if (uv_indices_ptr->size() != vertex_indices.size()) {
      std::cerr << "WARNING (file: " << file_name << "): ";
      std::cerr << "Some faces have uv indices, others don't. ";
      std::cerr << "This results in ignoring all uv indices.\n";
      uv_indices_ptr = nullptr;
    } else {
      for (unsigned i = 0; i < uv_indices_ptr->size(); ++i) {
        if ((*uv_indices_ptr)[i].size() != vertex_indices[i].size()) {
          std::cerr << "WARNING (file: " << file_name << "): ";
          std::cerr << "A face has uv indices for some but not all vertices. ";
          std::cerr << "This results in ignoring all uv indices.\n";
          uv_indices_ptr = nullptr;
          break;
        }
      }
    }
  }

  FromIndexedVertices(vertices, vertex_indices, normals_ptr, normal_indices_ptr,
                      uvs_ptr, uv_indices_ptr, &file_name);

  delete mesh;
  return false;
}

void HalfedgeMesh::TexProjectUVInUnitXY(const Affine3& tex_trafo) {
  Halfedge_property<Vec2> uv0s = mesh_.get_halfedge_property<Vec2>("h:uv0");

  const Vec3 trans = UnitTrafoTranslation();
  const Vec3 scale = UnitTrafoScale();
  for (Halfedge_iterator h_it = mesh_.halfedges_begin();
       h_it != mesh_.halfedges_end(); ++h_it) {
    Vec3 p_loc = mesh_.position(mesh_.to_vertex(*h_it));
    p_loc = scale.cwiseProduct(p_loc + trans);
    const Vec3 p = tex_trafo * p_loc;
    uv0s[*h_it] = Vec2(p.x(), p.y());
  }
  has_uv0_ = true;
}

void HalfedgeMesh::TexTransformUV(const Affine2& tex_trafo) {
  assert(has_uv0_);

  Halfedge_property<Vec2> uv0s = mesh_.get_halfedge_property<Vec2>("h:uv0");

  for (Halfedge_iterator h_it = mesh_.halfedges_begin();
       h_it != mesh_.halfedges_end(); ++h_it) {
    uv0s[*h_it] = tex_trafo * uv0s[*h_it];
  }
}

void HalfedgeMesh::FlipWinding(bool flip_normals) {
  // This is probably not the best way to do this, but whatever, it works.
  SurfaceMesh tmp;
  Face_property<Vec3> f_normals_new = tmp.add_face_property<Vec3>("f:normal");
  Halfedge_property<Vec3> fv_normals_new =
      tmp.add_halfedge_property<Vec3>("h:normal");
  Halfedge_property<Vec2> uv0s_new = tmp.add_halfedge_property<Vec2>("h:uv0");

  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");
  Halfedge_property<Vec3> fv_normals =
      mesh_.get_halfedge_property<Vec3>("h:normal");
  Halfedge_property<Vec2> uv0s = mesh_.get_halfedge_property<Vec2>("h:uv0");

  for (Vertex_iterator v_it = mesh_.vertices_begin();
       v_it != mesh_.vertices_end(); ++v_it) {
    tmp.add_vertex(mesh_.position(*v_it));
  }

  for (Face_iterator f_it = mesh_.faces_begin(); f_it != mesh_.faces_end();
       ++f_it) {
    Vertex_around_face_circulator vf_it, vf_it_end;
    vf_it = vf_it_end = mesh_.vertices(*f_it);
    std::vector<Vertex> verts;
    do {
      verts.push_back(*vf_it);
    } while (++vf_it != vf_it_end);
    std::reverse(verts.begin(), verts.end());
    Face new_face = tmp.add_face(verts);
    f_normals_new[new_face] = (flip_normals ? -1.0 : 1.0) * f_normals[*f_it];

    if (has_face_vertex_normals_) {
      Halfedge_around_face_circulator hf_it, hf_it_end;
      hf_it = hf_it_end = mesh_.halfedges(*f_it);
      Vec3Vec normals;
      do {
        normals.push_back((flip_normals ? -1.0 : 1.0) * fv_normals[*hf_it]);
      } while (++hf_it != hf_it_end);
      std::reverse(normals.begin(), normals.end());

      hf_it = hf_it_end = tmp.halfedges(new_face);
      unsigned v_idx = 0;
      do {
        fv_normals_new[*hf_it] = normals[v_idx++];
      } while (++hf_it != hf_it_end);
    }

    if (has_uv0_) {
      Halfedge_around_face_circulator hf_it, hf_it_end;
      hf_it = hf_it_end = mesh_.halfedges(*f_it);
      Vec2Vec face_uvs;
      do {
        face_uvs.push_back(uv0s[*hf_it]);
      } while (++hf_it != hf_it_end);
      std::reverse(face_uvs.begin(), face_uvs.end());

      hf_it = hf_it_end = tmp.halfedges(new_face);
      unsigned v_idx = 0;
      do {
        uv0s_new[*hf_it] = face_uvs[v_idx++];
      } while (++hf_it != hf_it_end);
    }
  }
  mesh_ = tmp;
}

void HalfedgeMesh::Mirror(const Plane3& mirror_plane) {
  // Mirror each point by the plane.
  for (Vertex_iterator v_it = mesh_.vertices_begin();
       v_it != mesh_.vertices_end(); ++v_it) {
    Vec3& p = mesh_.position(*v_it);
    p -= 2.0 * mirror_plane.Distance(p) * mirror_plane.normal;
  }

  // Mirror all face normals.
  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");
  Halfedge_property<Vec3> fv_normals =
      mesh_.get_halfedge_property<Vec3>("h:normal");

  for (Face_iterator f_it = mesh_.faces_begin(); f_it != mesh_.faces_end();
       ++f_it) {
    // Relfect about plane through origin (ignore 4th component).
    f_normals[*f_it] -=
        2.0 * mirror_plane.normal.dot(f_normals[*f_it]) * mirror_plane.normal;

    // Mirror all face vertex normals.
    if (has_face_vertex_normals_) {
      Halfedge_around_face_circulator hf_it, hf_it_end;
      hf_it = hf_it_end = mesh_.halfedges(*f_it);
      do {
        // Relfect about plane through origin (ignore 4th component).
        fv_normals[*hf_it] -= 2.0 *
                              mirror_plane.normal.dot(fv_normals[*hf_it]) *
                              mirror_plane.normal;
      } while (++hf_it != hf_it_end);
    }
  }

  FlipWinding(false);

  unit_trafo_dirty_ = true;
}

Scalar HalfedgeMesh::GetFaceArea(unsigned idx) const {
  assert(idx < mesh_.n_faces());

  Vertex_around_face_circulator vf_it, vf_it_end;
  vf_it = vf_it_end = mesh_.vertices(Face(idx));
  Vec3 normal = Vec3::Zero();
  do {
    const Vec3& p = mesh_.position(*vf_it);
    ++vf_it;
    const Vec3& pn = mesh_.position(*vf_it);
    normal[0] += (p[1] - pn[1]) * (p[2] + pn[2]);
    normal[1] += (p[2] - pn[2]) * (p[0] + pn[0]);
    normal[2] += (p[0] - pn[0]) * (p[1] + pn[1]);
  } while (vf_it != vf_it_end);

  return 0.5 * normal.norm();
}

bool HalfedgeMesh::ExtrudeAlongNormal(Scalar length) {
  if (mesh_.n_faces() != 1) {
    assert(false);
    return true;
  }

  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");
  return ExtrudeAlongDirection(f_normals[*mesh_.faces_begin()], length);
}

bool HalfedgeMesh::ExtrudeAlongDirection(const Vec3& direction, Scalar length) {
  if (mesh_.n_faces() != 1) {
    assert(false);
    return true;
  }

  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");
  const Vec3 offset = length * direction.normalized();
  Face orig_face = *mesh_.faces_begin();
  if (f_normals[orig_face].dot(direction.normalized()) < EPSILON) {
    return true;
  }

  Vertex_around_face_circulator vf_it, vf_it_end;
  vf_it = vf_it_end = mesh_.vertices(orig_face);
  std::vector<Vertex> top, bottom;
  do {
    bottom.push_back(*vf_it);
    top.push_back(mesh_.add_vertex(mesh_.position(*vf_it) + offset));
  } while (++vf_it != vf_it_end);

  Face top_face = mesh_.add_face(top);
  f_normals[top_face] = f_normals[orig_face];

  std::vector<Vertex> new_bottom = bottom;
  std::reverse(new_bottom.begin(), new_bottom.end());
  Face bottom_face = mesh_.add_face(new_bottom);
  f_normals[bottom_face] = -f_normals[orig_face];
  mesh_.delete_face(orig_face);

  // Generate sides.
  for (unsigned i = 0; i < bottom.size(); ++i) {
    unsigned i_n = (i + 1) % bottom.size();
    Face new_face = mesh_.add_quad(bottom[i], bottom[i_n], top[i_n], top[i]);
    f_normals[new_face] = mesh_.compute_face_normal(new_face);
  }

  mesh_.garbage_collection();
  unit_trafo_dirty_ = true;
  has_face_vertex_normals_ = false;
  has_uv0_ = false;

  return false;
}

static void RotatedPolygon2(const SurfaceMesh& mesh, Mat3* rot, Scalar* height,
                            Vec2Vec* polygon) {
  // Create rotation matrix to bring all the points into 2D coordinates.
  Face_property<Vec3> f_normals = mesh.get_face_property<Vec3>("f:normal");
  Face orig_face = *mesh.faces_begin();
  const Vec3 normal = f_normals[orig_face];
  Vec3 new_z = Vec3::UnitX().cross(normal);
  Vec3 tmp = Vec3::UnitY().cross(normal);
  if (tmp.squaredNorm() > new_z.squaredNorm()) {
    new_z = tmp;
  }
  tmp = Vec3::UnitZ().cross(normal);
  if (tmp.squaredNorm() > new_z.squaredNorm()) {
    new_z = tmp;
  }
  new_z.normalize();
  Vec3 new_x = normal.cross(new_z);
  new_x.normalize();  // This should already be normalized.

  rot->row(0) = new_x;
  rot->row(1) = normal;
  rot->row(2) = new_z;

  Vertex_around_face_circulator vf_it, vf_it_end;
  vf_it = vf_it_end = mesh.vertices(orig_face);
  polygon->clear();
  *height = 0.0;
  bool first = true;
  do {
    Vec3 tmp = *rot * mesh.position(*vf_it);
    polygon->push_back(Vec2(tmp.x(), -tmp.z()));  // Map 3D to 2D.
    if (first) {
      *height = tmp.y();
      first = false;
    }
    assert(first || fabs(*height - tmp.y()) < EPSILON);
  } while (++vf_it != vf_it_end);
}

void HalfedgeMesh::ExtrudeRoofHipOrGable(Scalar angle, Scalar overhang_side,
                                         bool gable, Scalar overhang_gable) {
  assert(mesh_.n_faces() == 1);
  assert(overhang_side >= 0.0 && ((gable && overhang_gable >= 0.0) ||
                                  (!gable && overhang_gable == 0.0)));

  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");

  Face orig_face = *mesh_.faces_begin();
  const Vec3 up_dir_orig = f_normals[orig_face];
  const Scalar sin_alpha = sin(angle / 180.0 * M_PI);
  const unsigned n_vertices_orig = mesh_.n_vertices();

  Scalar height;
  Mat3 rot;
  Vec2Vec poly_2d;
  RotatedPolygon2(mesh_, &rot, &height, &poly_2d);
  const Mat3 rot_inv = rot.transpose();

  Vec3Vec verts_3d;
  IdxVecVec indices;
  skeleton::Skeleton skeleton(poly_2d, {});
  skeleton::Roof(skeleton, angle, gable, false, &verts_3d, &indices);
  for (unsigned i = 0; i < verts_3d.size(); ++i) {
    verts_3d[i] = rot_inv * (verts_3d[i] + Vec3(0.0, height, 0.0));
  }
  FromIndexedVertices(verts_3d, indices, nullptr, nullptr, nullptr, nullptr,
                      nullptr);

  // Create the overhangs.
  if (overhang_side > 0.0 || (gable && overhang_gable > 0.0)) {
    // Delete bottom face.
    mesh_.delete_face(Face(mesh_.n_faces() - 1));

    Vec3Vec offsets(mesh_.n_vertices(), Vec3::Zero());

    int start_idx = 0;
    while (mesh_.valence(Vertex(start_idx)) == 2) {
      ++start_idx;
    }
    assert(mesh_.valence(Vertex(start_idx)) == 3);

    for (unsigned i = 0; i < n_vertices_orig; ++i) {
      Vertex v((i + start_idx) % n_vertices_orig);
      assert(mesh_.valence(v) == 2 || mesh_.valence(v) == 3);
      Halfedge h(mesh_.halfedge(v));
      assert(mesh_.is_boundary(h));
      h = mesh_.opposite_halfedge(h);

      Vertex vp = mesh_.from_vertex(h);
      Face f(mesh_.face(h));
      Vec3 face_normal = f_normals[f];
      Vec3 p = mesh_.position(v);
      Vec3 pp = mesh_.position(vp);
      Vec3 orth_offset_dir = (p - pp).cross(f_normals[f]).normalized();

      if (fabs(face_normal.dot(up_dir_orig)) < EPSILON) {
        std::vector<Vertex> new_face;
        new_face.push_back(mesh_.add_vertex(pp));
        new_face.push_back(mesh_.add_vertex(p));
        while (mesh_.valence(v) == 2) {
          h = mesh_.next_halfedge(h);
          v = mesh_.to_vertex(h);
          p = mesh_.position(v);
          new_face.push_back(mesh_.add_vertex(p));
          ++i;
        }
        Vertex v_top = mesh_.to_vertex(mesh_.next_halfedge(h));
        Vec3 p_top = mesh_.position(v_top);
        Vec3 roof_edge_dir = (p - mesh_.position(v_top)).normalized();

        new_face.push_back(mesh_.add_vertex(p_top));
        mesh_.delete_face(f);
        f_normals[mesh_.add_face(new_face)] = face_normal;

        offsets[v.idx()] = roof_edge_dir * sin_alpha * overhang_side /
                           -roof_edge_dir.dot(up_dir_orig);
        if (overhang_gable > 0.0) {
          h = mesh_.next_halfedge(h);  // The halfedge pointing to the top.
          Vertex next =
              mesh_.to_vertex(mesh_.next_halfedge(mesh_.opposite_halfedge(h)));
          Vertex top_prev = mesh_.from_vertex(
              mesh_.prev_halfedge(mesh_.opposite_halfedge(h)));
          Vertex prev_prev = mesh_.from_vertex(mesh_.prev_halfedge(
              mesh_.opposite_halfedge(mesh_.next_halfedge(h))));
          Vec3 next_dir = (p - mesh_.position(next)).normalized();
          Vec3 top_dir = (p_top - mesh_.position(top_prev)).normalized();
          Vec3 prev_dir = (pp - mesh_.position(prev_prev)).normalized();
          offsets[v.idx()] +=
              next_dir / next_dir.dot(face_normal) * overhang_gable;
          offsets[v_top.idx()] +=
              top_dir / top_dir.dot(face_normal) * overhang_gable;
          offsets[vp.idx()] +=
              prev_dir / prev_dir.dot(face_normal) * overhang_gable;
        }
      } else {
        if (mesh_.valence(v) == 2) {
          offsets[v.idx()] += orth_offset_dir * overhang_side;
        } else {
          Vertex v_top = mesh_.to_vertex(mesh_.next_halfedge(h));
          Vec3 roof_edge_dir = (p - mesh_.position(v_top)).normalized();
          offsets[v.idx()] += roof_edge_dir /
                              roof_edge_dir.dot(orth_offset_dir) *
                              overhang_side;
        }
      }
    }

    for (unsigned i = 0; i < offsets.size(); ++i) {
      mesh_.position(Vertex(i)) += offsets[i];
    }
  }

  mesh_.garbage_collection();
  unit_trafo_dirty_ = true;
  has_face_vertex_normals_ = false;
  has_uv0_ = false;
}

void HalfedgeMesh::ExtrudeRoofPyramid(Scalar height, Scalar overhang_height) {
  assert(mesh_.n_faces() == 1);

  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");

  // Use center of mass to generate the pyramid.
  Face orig_face = *mesh_.faces_begin();
  Vertex_around_face_circulator vf_it, vf_it_end;
  vf_it = vf_it_end = mesh_.vertices(orig_face);
  Vec3 center = Vec3::Zero();
  do {
    center += mesh_.position(*vf_it);
  } while (++vf_it != vf_it_end);

  // Average it. Let's hope that all vertices are in the one face and that we
  // don't have singular vertices, in which case this would all break.
  center /= mesh_.n_vertices();
  center += height * f_normals[orig_face];
  Vertex top = mesh_.add_vertex(center);

  //
  // Add bottom face.
  vf_it = vf_it_end = mesh_.vertices(orig_face);
  std::vector<Vertex> bottom;
  do {
    bottom.push_back(*vf_it);
  } while (++vf_it != vf_it_end);

  std::vector<Vertex> new_bottom = bottom;
  std::reverse(new_bottom.begin(), new_bottom.end());
  Face bottom_face = mesh_.add_face(new_bottom);
  f_normals[bottom_face] = -f_normals[orig_face];

  mesh_.delete_face(orig_face);

  // Generate sides.
  for (size_t i = 0; i < bottom.size(); ++i) {
    const size_t i_n = (i + 1) % bottom.size();
    const Face new_face = mesh_.add_triangle(bottom[i], bottom[i_n], top);
    f_normals[new_face] = mesh_.compute_face_normal(new_face);
  }

  if (overhang_height > 0.0) {
    vf_it = vf_it_end = mesh_.vertices(bottom_face);
    do {
      const Vec3 pos = mesh_.position(*vf_it);
      const Vec3 dir = (pos - center).normalized();
      const Scalar dot = dir.dot(f_normals[bottom_face]);
      assert(dot > 0.0);
      mesh_.position(*vf_it) = pos + dir / dot;
    } while (++vf_it != vf_it_end);

    // Delete the bottom face again.
    mesh_.delete_face(bottom_face);
  }

  mesh_.garbage_collection();
  unit_trafo_dirty_ = true;
  has_face_vertex_normals_ = false;
  has_uv0_ = false;
}

void HalfedgeMesh::ExtrudeRoofShed(Scalar angle) {
  assert(mesh_.n_faces() == 1);

  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");

  Face orig_face = *mesh_.faces_begin();
  Vertex_around_face_circulator vf_it, vf_it_end;
  vf_it = vf_it_end = mesh_.vertices(orig_face);
  const Vec3 normal = f_normals[orig_face];
  std::vector<Vertex> bottom, top;
  do {
    bottom.push_back(*vf_it);

    Vec3 pos = mesh_.position(*vf_it);
    if (pos.x() > EPSILON) {
      pos += normal * pos.x() * tan(M_PI / 180.0 * angle);
      top.push_back(mesh_.add_vertex(pos));
    } else {
      top.push_back(*vf_it);
    }
  } while (++vf_it != vf_it_end);

  // Add bottom face.
  std::vector<Vertex> new_bottom = bottom;
  std::reverse(new_bottom.begin(), new_bottom.end());
  Face bottom_face = mesh_.add_face(new_bottom);
  f_normals[bottom_face] = -f_normals[orig_face];

  mesh_.delete_face(orig_face);

  // Add top face
  Face top_face = mesh_.add_face(top);
  f_normals[top_face] = mesh_.compute_face_normal(top_face);

  // Generate sides.
  for (size_t i = 0; i < bottom.size(); ++i) {
    const size_t i_n = (i + 1) % bottom.size();
    std::vector<Vertex> side;

    side.push_back(bottom[i]);
    side.push_back(bottom[i_n]);
    if (top[i_n] != bottom[i_n]) {
      side.push_back(top[i_n]);
    }
    if (top[i] != bottom[i]) {
      side.push_back(top[i]);
    }

    if (side.size() > 2) {
      Face new_face = mesh_.add_face(side);
      f_normals[new_face] = mesh_.compute_face_normal(new_face);
    }
  }

  mesh_.garbage_collection();
  unit_trafo_dirty_ = true;
  has_face_vertex_normals_ = false;
  has_uv0_ = false;
}

// Helper enum for mesh splitting.
namespace {

enum SplitSide : char {
  BELOW,
  ON,
  ABOVE,
};

}

// Helper function that makes caps to close a meshes that were cut open.
static void MakeCaps(SurfaceMesh* mesh, bool has_face_vertex_normals,
                     bool has_uv0s) {
  Edge_property<SplitSide> e_class =
      mesh->get_edge_property<SplitSide>("e:split_classifier");
  Face_property<Vec3> f_normals = mesh->get_face_property<Vec3>("f:normal");
  Halfedge_property<Vec3> fv_normals =
      mesh->get_halfedge_property<Vec3>("h:normal");
  Halfedge_property<Vec2> uv0s = mesh->get_halfedge_property<Vec2>("h:uv0");
  std::set<Edge> used_edges;

  for (Edge_iterator e_it = mesh->edges_begin(); e_it != mesh->edges_end();
       ++e_it) {
    // Only evaluate ON eges, only they can form caps.
    if (e_class[*e_it] != ON || used_edges.find(*e_it) != used_edges.end()) {
      continue;
    }
    Halfedge start = mesh->halfedge(*e_it, 0);
    if (!mesh->is_boundary(start)) {
      start = mesh->opposite_halfedge(start);
      if (!mesh->is_boundary(start)) {
        continue;
      }
    }

    std::vector<Vertex> cap;
    Vec2Vec uvs;
    Halfedge h = start;
    do {
      cap.push_back(mesh->from_vertex(h));
      if (has_uv0s) {
        uvs.push_back(uv0s[mesh->opposite_halfedge(h)]);
      }
      h = mesh->next_halfedge(h);
      assert(mesh->is_boundary(h));
      used_edges.insert(mesh->edge(h));
    } while (e_class[mesh->edge(h)] == ON && h != start);
    if (h == start) {
      Face new_face = mesh->add_face(cap);
      f_normals[new_face] = mesh->compute_face_normal(new_face);

      // Cap normals. Is this ok?
      if (has_face_vertex_normals) {
        Halfedge_around_face_circulator hf_it, hf_it_end;
        hf_it = hf_it_end = mesh->halfedges(new_face);
        do {
          fv_normals[*hf_it] = f_normals[new_face];
        } while (++hf_it != hf_it_end);
      }
      if (has_uv0s) {
        Halfedge_around_face_circulator hf_it, hf_it_end;
        hf_it = hf_it_end = mesh->halfedges(new_face);
        unsigned uv_idx = 0;
        do {
          uv0s[*hf_it] = uvs[uv_idx++];
        } while (++hf_it != hf_it_end);
      }
    }
  }
}

void HalfedgeMesh::Split(const Plane3& plane, HalfedgeMeshPtr* below,
                         HalfedgeMeshPtr* above) const {
  *below = std::make_shared<HalfedgeMesh>();
  *above = std::make_shared<HalfedgeMesh>();
  SurfaceMesh* b = &(*below)->mesh_;
  SurfaceMesh* a = &(*above)->mesh_;
  *b = mesh_;
  Vertex_property<SplitSide> v_class =
      b->add_vertex_property<SplitSide>("v:split_classifier");
  Edge_property<SplitSide> e_class =
      b->add_edge_property<SplitSide>("e:split_classifier");
  Face_property<SplitSide> f_class =
      b->add_face_property<SplitSide>("f:split_classifier");
  Face_property<Vec3> f_normals = b->get_face_property<Vec3>("f:normal");
  Halfedge_property<Vec3> fv_normals =
      b->get_halfedge_property<Vec3>("h:normal");
  Halfedge_property<Vec2> uv0s = b->get_halfedge_property<Vec2>("h:uv0");

  // Vertex classification
  for (Vertex_iterator v_it = b->vertices_begin(); v_it != b->vertices_end();
       ++v_it) {
    const Vec3& p = b->position(*v_it);
    if (plane.On(p)) {
      v_class[*v_it] = ON;
    } else if (plane.Above(p)) {
      v_class[*v_it] = ABOVE;
    } else {
      v_class[*v_it] = BELOW;
    }
  }

  // Edge splitting and classification
  Edge_iterator e_it, e_it_end = b->edges_end();
  for (e_it = b->edges_begin(); e_it != e_it_end; ++e_it) {
    Halfedge h0 = b->halfedge(*e_it, 0);
    Vertex from = b->from_vertex(h0);
    Vertex to = b->to_vertex(h0);

    // Check if the edge intersects the plane. If yes, split it.
    if ((v_class[from] == ABOVE && v_class[to] == BELOW) ||
        (v_class[from] == BELOW && v_class[to] == ABOVE)) {
      const Vec3 origin = b->position(from);
      const Vec3 dir = b->position(to) - origin;
      Vec3 intersection;
      Scalar lambda;
      if (!IntersectionRayPlane(Ray3(origin, dir), plane, &intersection,
                                &lambda)) {
        assert(!"ERROR: Couldn't find intersection that is supposed to exist!");
      }
      Vertex v_intersection = b->add_vertex(intersection);
      b->insert_vertex(h0, v_intersection);
      v_class[v_intersection] = ON;
      e_class[b->edge(h0)] = v_class[from];

      assert(v_class[from] != ON);
      e_class[b->edge(b->next_halfedge(h0))] =
          v_class[from] == ABOVE ? BELOW : ABOVE;

      // Interpolation of face vertex normals and texture coordinates.
      if (has_face_vertex_normals_ || has_uv0_) {
        assert(dir.norm() > EPSILON);
        const Scalar alpha = (intersection - origin).norm() / dir.norm();

        Halfedge h_start, h_end, h_middle;
        h_middle = h0;
        h_end = b->next_halfedge(h_middle);
        h_start = b->prev_halfedge(h_middle);
        if (has_face_vertex_normals_) {
          fv_normals[h_end] = fv_normals[h_middle];
          fv_normals[h_middle] =
              (fv_normals[h_start] * (1.0 - alpha) + fv_normals[h_end] * alpha)
                  .normalized();
        }
        if (has_uv0_) {
          uv0s[h_end] = uv0s[h_middle];
          uv0s[h_middle] = uv0s[h_start] * (1.0 - alpha) + uv0s[h_end] * alpha;
        }

        h_start = b->opposite_halfedge(h0);
        h_middle = b->prev_halfedge(h_start);
        h_end = b->prev_halfedge(h_middle);
        if (has_face_vertex_normals_) {
          fv_normals[h_middle] =
              (fv_normals[h_start] * (1.0 - alpha) + fv_normals[h_end] * alpha)
                  .normalized();
        }
        if (has_uv0_) {
          uv0s[h_middle] = uv0s[h_start] * (1.0 - alpha) + uv0s[h_end] * alpha;
        }
      }
    } else if (v_class[from] == ON && v_class[to] == ON) {
      e_class[*e_it] = ON;
    } else if (v_class[from] == ABOVE || v_class[to] == ABOVE) {
      e_class[*e_it] = ABOVE;
    } else {
      e_class[*e_it] = BELOW;
    }
  }

  // Face splitting and classification
  Halfedge_around_face_circulator hf_it, hf_it_end;
  for (Face_iterator f_it = b->faces_begin(); f_it != b->faces_end(); ++f_it) {
    const Face face = *f_it;
    hf_it = hf_it_end = b->halfedges(face);
    bool is_on = true;
    bool is_above = true;
    bool is_below = true;
    // is_on could be deteced more efficiently by only looking at the first 2
    // edges if we assume that a polygon is 'flat'.
    do {
      if (e_class[b->edge(*hf_it)] == ABOVE) {
        is_on = false;
        is_below = false;
      } else if (e_class[b->edge(*hf_it)] == BELOW) {
        is_on = false;
        is_above = false;
      }
    } while (++hf_it != hf_it_end);

    if (is_on) {
      // The face is coplanar to the split plane, we need to compare face and
      // split plane normal.
      if (plane.normal.dot(f_normals[face]) > 0.0) {
        f_class[face] = BELOW;
      } else {
        f_class[face] = ABOVE;
      }
    } else if (is_above) {
      f_class[face] = ABOVE;
    } else if (is_below) {
      f_class[face] = BELOW;
    } else {
      // There has to be at least one cut across the face. Find it/them.
      // We find them by projecting all crossing points onto the line at the
      // intersection of the splitting plane and the polygon plane, and by
      // sorting all crossings along the line. Two subsequent crossings in that
      // list are the end points of a new edge.
      std::vector<Halfedge> crossings;

      const Vec3 face_normal = f_normals[face];
      Halfedge h_prev, h_next = b->halfedge(face);
      do {
        h_prev = h_next;
        h_next = b->next_halfedge(h_next);
        if (v_class[b->to_vertex(h_prev)] != ON) {
          continue;
        }

        SplitSide side_prev = e_class[b->edge(h_prev)];
        SplitSide side_next = e_class[b->edge(h_next)];

        if ((side_prev == BELOW && side_next == ABOVE) ||
            (side_prev == ABOVE && side_next == BELOW)) {
          crossings.push_back(h_prev);
        } else {
          Vec3 prev = b->position(b->from_vertex(h_prev));
          Vec3 curr = b->position(b->to_vertex(h_prev));
          Vec3 next = b->position(b->to_vertex(h_next));

          // if (angle > 180.0deg)
          if (face_normal.dot((next - curr).cross(prev - curr)) < 0.0) {
            // TODO(stefalie): Think this through once more in detail. Doubling
            // the crossing is not a good idea. To insert edges we need two half
            // edges. After inserting one new edge at the crossing it is
            // possible that one of the two half edges for the second new edge
            // has changed.
            //
            // The problem are situations like:
            //   /| /|
            //  /_|/_|
            // where one of the vertices lies exactly on an edge. There is no
            // easy way with surface mesh to fix that case I think.
            //
            // crossings.push_back(h_prev);
            // if (side_prev != ON && side_next != ON) {
            //   // Double it if angle is > 180deg and both are on the same side
            //   // and not ON.
            //   crossings.push_back(h_prev);
            // }
            if (side_prev == ON || side_next == ON) {
              crossings.push_back(h_prev);
            }
          }
        }
      } while (h_next != b->halfedge(face));
      assert(crossings.size() % 2 == 0);

      // Sort crossings by position along the line.
      struct Compare {
        Vec3 dir;
        const SurfaceMesh* mesh;
        bool operator()(Halfedge e1, Halfedge e2) {
          return (mesh->position(mesh->to_vertex(e1)).dot(dir) <
                  mesh->position(mesh->to_vertex(e2)).dot(dir));
        }
      } cmp;
      cmp.dir = face_normal.cross(plane.normal);  // Projection direction
      cmp.mesh = b;
      std::sort(crossings.begin(), crossings.end(), cmp);

      // Insert the edges.
      std::set<Face> faces_to_label;
      faces_to_label.insert(face);
      for (unsigned i = 0; i < crossings.size(); i += 2) {
        Halfedge h_inserted = b->insert_edge(crossings[i], crossings[i + 1]);
        Halfedge h_inserted_opp = b->opposite_halfedge(h_inserted);
        e_class[b->edge(h_inserted)] = ON;
        Face new_face = b->face(h_inserted_opp);
        faces_to_label.insert(new_face);
        f_normals[new_face] = f_normals[face];
        if (has_face_vertex_normals_) {
          fv_normals[h_inserted] = fv_normals[crossings[i + 1]];
          fv_normals[h_inserted_opp] = fv_normals[crossings[i]];
        }
        if (has_uv0_) {
          uv0s[h_inserted] = uv0s[crossings[i + 1]];
          uv0s[h_inserted_opp] = uv0s[crossings[i]];
        }
      }

      // Label the faces.
      for (Face f : faces_to_label) {
        hf_it = hf_it_end = b->halfedges(f);
        do {
          SplitSide side = e_class[b->edge(*hf_it)];
          if (side != ON) {
            f_class[f] = side;
            break;
          }
        } while (++hf_it != hf_it_end);
      }
    }
  }

  // Delete the unused faces on both sides.
  b->remove_vertex_property(v_class);
  *a = *b;
  Edge_property<SplitSide> e_class_above =
      a->get_edge_property<SplitSide>("e:split_classifier");
  Face_property<SplitSide> f_class_above =
      a->get_face_property<SplitSide>("f:split_classifier");

  for (Face_iterator f_it = b->faces_begin(); f_it != b->faces_end(); ++f_it) {
    if (f_class[*f_it] == ABOVE) {
      b->delete_face(*f_it);
    }
  }
  for (Face_iterator f_it = a->faces_begin(); f_it != a->faces_end(); ++f_it) {
    if (f_class_above[*f_it] == BELOW) {
      a->delete_face(*f_it);
    }
  }

  // Create caps to close the holes and clean up.
  MakeCaps(b, has_face_vertex_normals_, has_uv0_);
  MakeCaps(a, has_face_vertex_normals_, has_uv0_);

  b->remove_edge_property(e_class);
  b->remove_face_property(f_class);
  b->garbage_collection();
  a->remove_edge_property(e_class_above);
  a->remove_face_property(f_class_above);
  a->garbage_collection();

  (*below)->unit_trafo_dirty_ = true;
  (*above)->unit_trafo_dirty_ = true;
  (*below)->has_face_vertex_normals_ = has_face_vertex_normals_;
  (*above)->has_face_vertex_normals_ = has_face_vertex_normals_;
  (*below)->has_uv0_ = has_uv0_;
  (*above)->has_uv0_ = has_uv0_;
}

const Vec3& HalfedgeMesh::GetFaceNormal(int face_idx) const {
  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");
  return f_normals[Face(face_idx)];
}

HalfedgeMeshPtr HalfedgeMesh::GetFaceComponent(int face_idx) const {
  Halfedge_property<Vec3> fv_normals =
      mesh_.get_halfedge_property<Vec3>("h:normal");
  Halfedge_property<Vec2> uv0s = mesh_.get_halfedge_property<Vec2>("h:uv0");

  Vec3Vec poly;
  Vec3Vec poly_normals;
  Vec2Vec poly_uvs;
  Vec3Vec* poly_normals_ptr =
      has_face_vertex_normals_ ? &poly_normals : nullptr;
  Vec2Vec* poly_uvs_ptr = has_uv0_ ? &poly_uvs : nullptr;

  const Face face(face_idx);
  Halfedge_around_face_circulator hf_it, hf_it_end;
  hf_it = hf_it_end = mesh_.halfedges(face);
  do {
    poly.push_back(mesh_.position(mesh_.to_vertex(*hf_it)));
    if (has_face_vertex_normals_) {
      poly_normals.push_back(fv_normals[*hf_it]);
    }
    if (has_uv0_) {
      poly_uvs.push_back(uv0s[*hf_it]);
    }
  } while (++hf_it != hf_it_end);

  HalfedgeMeshPtr new_mesh = std::make_shared<HalfedgeMesh>();
  new_mesh->FromPolygon(poly, poly_normals_ptr, poly_uvs_ptr);
  return new_mesh;
}

// For fixed floating point.
static inline int ffp(Scalar t) {
  assert(fabs(t * (1 << 16) + 0.5) <= INT_MAX);
  return static_cast<int>(t * (1 << 16) + 0.5);
}

void HalfedgeMesh::FillRenderBuffer(std::vector<RenderVertex>* vertex_data,
                                    IdxVec* indices) const {
  assert(vertex_data && vertex_data->empty());
  assert(indices && indices->empty());

  struct RenderVertexInt {
    bool operator==(const RenderVertexInt& other) const {
      return (position_i == other.position_i && normal_i == other.normal_i &&
              uv_i == other.uv_i);
    }

    Vec3i position_i;
    Vec3i normal_i;
    Vec2i uv_i;
  };
  struct RenderVertexIntHash {
    std::size_t operator()(const RenderVertexInt& v) const {
      return (
          ((Vec3iHash()(v.position_i) ^ (Vec3iHash()(v.normal_i) << 1)) >> 1) ^
          Vec2iHash()(v.uv_i));
    }
  };
  std::unordered_map<RenderVertexInt, int, RenderVertexIntHash> vertex_cache;

  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");
  Halfedge_property<Vec3> fv_normals =
      mesh_.get_halfedge_property<Vec3>("h:normal");
  Halfedge_property<Vec2> uv0s = mesh_.get_halfedge_property<Vec2>("h:uv0");

  Halfedge_around_face_circulator hf_it, hf_it_end;
  for (Face_iterator f_it = mesh_.faces_begin(); f_it != mesh_.faces_end();
       ++f_it) {
    Vec3Vec face_vertices;
    face_vertices.reserve(10);
    Vec3Vec face_normals;
    face_normals.reserve(10);
    Vec2Vec face_uvs;
    face_uvs.reserve(10);
    hf_it = hf_it_end = mesh_.halfedges(*f_it);
    do {
      face_vertices.push_back(mesh_.position(mesh_.to_vertex(*hf_it)));
      if (has_face_vertex_normals_) {
        face_normals.push_back(fv_normals[*hf_it]);
      } else {
        face_normals = Vec3Vec(face_vertices.size(), f_normals[*f_it]);
      }

      if (has_uv0_) {
        face_uvs.push_back(uv0s[*hf_it]);
      }
    } while (++hf_it != hf_it_end);

    // Check if the vertex is already present in the cache.
    std::unordered_map<int, int> face_idx_to_global_idx;
    face_idx_to_global_idx.reserve(2048);

    for (int i = 0; i < static_cast<int>(face_vertices.size()); ++i) {
      RenderVertexInt rv_i;
      rv_i.position_i =
          Vec3i(ffp(face_vertices[i].x()), ffp(face_vertices[i].y()),
                ffp(face_vertices[i].z()));
      rv_i.normal_i = Vec3i(ffp(face_normals[i].x()), ffp(face_normals[i].y()),
                            ffp(face_normals[i].z()));
      rv_i.uv_i = has_uv0_ ? Vec2i(ffp(face_uvs[i].x()), ffp(face_uvs[i].y()))
                           : Vec2i::Zero();

      auto it =
          vertex_cache.emplace(rv_i, static_cast<int>(vertex_data->size()));
      if (it.second) {
        RenderVertex rv;
        rv.position[0] = static_cast<float>(face_vertices[i].x());
        rv.position[1] = static_cast<float>(face_vertices[i].y());
        rv.position[2] = static_cast<float>(face_vertices[i].z());
        rv.normal[0] = static_cast<float>(face_normals[i].x());
        rv.normal[1] = static_cast<float>(face_normals[i].y());
        rv.normal[2] = static_cast<float>(face_normals[i].z());
        rv.uv[0] = has_uv0_ ? static_cast<float>(face_uvs[i].x()) : 0.0f;
        rv.uv[1] = has_uv0_ ? static_cast<float>(face_uvs[i].y()) : 0.0f;

        vertex_data->push_back(rv);
      }
      face_idx_to_global_idx.emplace(i, it.first->second);
    }

    // Triangulization.
    IdxVecVec tris;
    Triangulator3D triangulator;
    triangulator.Triangulize(face_vertices, &tris, true);
    for (size_t i = 0; i < tris.size(); ++i) {
      for (size_t j = 0; j < 3; ++j) {
        const int idx = tris[i][j];
        indices->push_back(face_idx_to_global_idx[idx]);
      }
    }
  }
}

void HalfedgeMesh::FillExportBuffers(Vec3Vec* vertices, Vec3Vec* normals,
                                     Vec2Vec* uvs, IdxVec* vertex_indices,
                                     IdxVec* normal_indices,
                                     IdxVec* uv_indices) const {
  assert(vertices->empty());
  assert(vertices->empty());
  assert(normals->empty());
  assert(vertex_indices->empty());
  assert(normal_indices->empty());
  assert(uvs->empty());
  assert(uv_indices->empty());

  // A cache is used to keep track of and merge normals and uv coordinates that
  // appear more than once. For vertex positions that does likely not make much
  // sense with a halfedge data structure because if duplicate vertices exist,
  // then that's most likely intended. If wanted, vertices can still be merged
  // later on in the exporter later that prepares the data for mesh file.
  //
  // To prevent floating point precision issues, we use integer coordinates as
  // hash keys into the cash. The floats are simply scaled by 2^16 and rounded
  // to the closest int. Integer overflow will occur if we have someting with
  // dimension larger than 2^16, i.e., larger than ~65km (assuming our units to
  // be in meters). The rounding error will at most be 0.5/2^16 ~= 0.008 mm,
  // so that should be okay. I got the idea from:
  // https://github.com/sylefeb/VoxSurf/blob/master/main.cpp

  // Map points to their indices the the merged array.
  std::unordered_map<Vec3i, int, Vec3iHash> normal_to_merged_idx;
  std::unordered_map<Vec2i, int, Vec2iHash> uv_to_merged_idx;

  // Map the indices used in the halfedge mesh into indices into
  // the merged array.
  std::unordered_map<int, int>
      normal_idx_to_merged_idx;  // Used for fv or f normals, not both.
  std::unordered_map<int, int> uv0_idx_to_merged_idx;

  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");
  Halfedge_property<Vec3> fv_normals =
      mesh_.get_halfedge_property<Vec3>("h:normal");
  Halfedge_property<Vec2> uv0s = mesh_.get_halfedge_property<Vec2>("h:uv0");

  for (Vertex_iterator v_it = mesh_.vertices_begin();
       v_it != mesh_.vertices_end(); ++v_it) {
    vertices->push_back(mesh_.position(*v_it));
  }

  if (has_face_vertex_normals_) {
    for (Halfedge_iterator h_it = mesh_.halfedges_begin();
         h_it != mesh_.halfedges_end(); ++h_it) {
      if (!mesh_.is_boundary(*h_it)) {
        const Vec3& n = fv_normals[*h_it];
        const Vec3i n_i(ffp(n.x()), ffp(n.y()), ffp(n.z()));

        auto it = normal_to_merged_idx.emplace(
            n_i, static_cast<int>(normals->size()));
        if (it.second) {
          normals->push_back(n);
        }
        normal_idx_to_merged_idx.emplace((*h_it).idx(), it.first->second);
      }
    }
  } else {
    for (Face_iterator f_it = mesh_.faces_begin(); f_it != mesh_.faces_end();
         ++f_it) {
      const Vec3& n = f_normals[*f_it];
      const Vec3i n_i(ffp(n.x()), ffp(n.y()), ffp(n.z()));

      auto it =
          normal_to_merged_idx.emplace(n_i, static_cast<int>(normals->size()));
      if (it.second) {
        normals->push_back(n);
      }
      normal_idx_to_merged_idx.emplace((*f_it).idx(), it.first->second);
    }
  }

  if (has_uv0_) {
    for (Halfedge_iterator h_it = mesh_.halfedges_begin();
         h_it != mesh_.halfedges_end(); ++h_it) {
      if (!mesh_.is_boundary(*h_it)) {
        const Vec2& uv = uv0s[*h_it];
        const Vec2i uv_i(ffp(uv.x()), ffp(uv.y()));

        auto it = uv_to_merged_idx.emplace(uv_i, static_cast<int>(uvs->size()));
        if (it.second) {
          uvs->push_back(uv);
        }
        uv0_idx_to_merged_idx.emplace((*h_it).idx(), it.first->second);
      }
    }
  }

  Halfedge_around_face_circulator hf_it, hf_it_end;
  for (Face_iterator f_it = mesh_.faces_begin(); f_it != mesh_.faces_end();
       ++f_it) {
    IdxVec face_vertex_indices, face_normals_indices, face_uv_indices;
    Vec3Vec face;
    hf_it = hf_it_end = mesh_.halfedges(*f_it);
    do {
      const Vertex vertex = mesh_.to_vertex(*hf_it);
      face.push_back(mesh_.position(vertex));
      face_vertex_indices.push_back(vertex.idx());

      if (has_face_vertex_normals_) {
        face_normals_indices.push_back(
            normal_idx_to_merged_idx[(*hf_it).idx()]);
      } else {
        face_normals_indices.push_back(normal_idx_to_merged_idx[(*f_it).idx()]);
      }

      if (has_uv0_) {
        face_uv_indices.push_back(uv0_idx_to_merged_idx[(*hf_it).idx()]);
      }
    } while (++hf_it != hf_it_end);

    IdxVecVec tris;
    Triangulator3D triangulator;
    triangulator.Triangulize(face, &tris, true);
    for (size_t i = 0; i < tris.size(); ++i) {
      for (size_t j = 0; j < 3; ++j) {
        vertex_indices->push_back(face_vertex_indices[tris[i][j]]);
        normal_indices->push_back(face_normals_indices[tris[i][j]]);
        if (has_uv0_) {
          uv_indices->push_back(face_uv_indices[tris[i][j]]);
        }
      }
    }
  }
}

// This is rather inefficient. I wonder if we couldn't direclty access the
// container of the SurfaceMesh.
void HalfedgeMesh::FillAABB(AABB* aabb) const {
  Vec3Vec verts;
  verts.reserve(mesh_.n_vertices());
  for (Vertex_iterator v_it = mesh_.vertices_begin();
       v_it != mesh_.vertices_end(); ++v_it) {
    verts.push_back(mesh_.position(*v_it));
  }
  *aabb = AABB(verts);
}

void HalfedgeMesh::Transform(const Isometry3& trafo, const Vec3& scale) {
  for (Vertex_iterator v_it = mesh_.vertices_begin();
       v_it != mesh_.vertices_end(); ++v_it) {
    mesh_.position(*v_it) = trafo * scale.cwiseProduct(mesh_.position(*v_it));
  }

  // normal_matrix = ((R * S) ^ -1) ^ T
  // = (S^-1 * T^-1) ^ T
  // = R^-T * S^-T
  // = R * S^-1
  const Mat3 normal_matrix = trafo.linear() * NormalMatrixFromScale(scale);

  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");
  for (Face_iterator f_it = mesh_.faces_begin(); f_it != mesh_.faces_end();
       ++f_it) {
    f_normals[*f_it] = (normal_matrix * f_normals[*f_it]).normalized();
  }

  if (has_face_vertex_normals_) {
    Halfedge_property<Vec3> fv_normals =
        mesh_.get_halfedge_property<Vec3>("h:normal");
    for (Halfedge_iterator h_it = mesh_.halfedges_begin();
         h_it != mesh_.halfedges_end(); ++h_it) {
      fv_normals[*h_it] = (normal_matrix * fv_normals[*h_it]).normalized();
    }
  }

  unit_trafo_dirty_ = true;
}

void HalfedgeMesh::TransformUnitTrafoAndScale(const Vec3& scale) {
  Isometry3 translation;
  Vec3 combined_scale;
  GetScaledUnitTrafo(scale, &translation, &combined_scale);
  Transform(translation, combined_scale);
}

void HalfedgeMesh::ComputeFaceVertexNormals() {
  HalfedgeMesh::has_face_vertex_normals_ = true;

  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");
  Halfedge_property<Vec3> fv_normals =
      mesh_.get_halfedge_property<Vec3>("h:normal");

  Face_property<Scalar> f_area = mesh_.add_face_property<Scalar>("f:area");

  for (Face_iterator f_it = mesh_.faces_begin(); f_it != mesh_.faces_end();
       ++f_it) {
    f_area[*f_it] = GetFaceArea((*f_it).idx());
  }

  for (Vertex_iterator v_it = mesh_.vertices_begin();
       v_it != mesh_.vertices_end(); ++v_it) {
    Vec3 normal = Vec3::Zero();

    Face_around_vertex_circulator fv_it, fv_it_end;
    fv_it = fv_it_end = mesh_.faces(*v_it);
    do {
      normal += f_normals[*fv_it] * f_area[*fv_it];
    } while (++fv_it != fv_it_end);

    normal.normalize();

    Halfedge_around_vertex_circulator hv_it, hv_it_end;
    hv_it = hv_it_end = mesh_.halfedges(*v_it);
    do {
      fv_normals[mesh_.opposite_halfedge(*hv_it)] = normal;
    } while (++hv_it != hv_it_end);
  }

  mesh_.remove_face_property(f_area);
}

// Trilinear interpolation for a 2x2x2 FFD. A more general version is below.
Vec3 TrilinearInterpolation(const Vec3Vec& cps, const Vec3& stu) {
  const Vec3& p000 = cps[0];
  const Vec3& p100 = cps[1];
  const Vec3& p010 = cps[2];
  const Vec3& p110 = cps[3];
  const Vec3& p001 = cps[4];
  const Vec3& p101 = cps[5];
  const Vec3& p011 = cps[6];
  const Vec3& p111 = cps[7];
  Scalar s = stu.x();
  Scalar t = stu.y();
  Scalar u = stu.z();
  return (1.0 - s) * ((1.0 - t) * ((1.0 - u) * p000 + u * p001) +
                      t * ((1.0 - u) * p010 + u * p011)) +
         s * ((1.0 - t) * ((1.0 - u) * p100 + u * p101) +
              t * ((1.0 - u) * p110 + u * p111));
}

int BinominalCoefficient(int n, int k) {
  assert(n >= k);

  k = k > n - k ? n - k : k;

  int ans = 1;
  for (int j = 1; j <= k; ++j, --n) {
    ans *= n;
    ans /= j;
  }

  assert(ans > 0);
  return ans;
}

Vec3 FFDInterpolation(const Vec3Vec& cps, unsigned l, unsigned m, unsigned n,
                      const Vec3& stu) {
  if (l == 1 && m == 1 && n == 1) {
    return TrilinearInterpolation(cps, stu);
  }

  // One to one from: Sederberg, 1986: Free-Form Deformation of Solid Geometric
  // Models Definitely not the most efficient way to do this, it recomputes
  // several of the values several times.
  const Scalar s = stu.x();
  const Scalar t = stu.y();
  const Scalar u = stu.z();

  ScalarVec weights_s, weights_t, weights_u;

  for (unsigned i = 0; i <= l; ++i) {
    Scalar weight = 1.0;
    for (unsigned c = 0; c < l; ++c) {
      if (c < l - i) {
        weight *= 1 - s;
      } else {
        weight *= s;
      }
    }
    weights_s.push_back(weight);
  }
  for (unsigned j = 0; j <= m; ++j) {
    Scalar weight = 1.0;
    for (unsigned c = 0; c < m; ++c) {
      if (c < m - j) {
        weight *= 1 - t;
      } else {
        weight *= t;
      }
    }
    weights_t.push_back(weight);
  }
  for (unsigned k = 0; k <= n; ++k) {
    Scalar weight = 1.0;
    for (unsigned c = 0; c < n; ++c) {
      if (c < n - k) {
        weight *= 1 - u;
      } else {
        weight *= u;
      }
    }
    weights_u.push_back(weight);
  }

  Vec3 res = Vec3::Zero();
  for (unsigned k = 0; k <= n; ++k) {
    for (unsigned j = 0; j <= m; ++j) {
      for (unsigned i = 0; i <= l; ++i) {
        res += (BinominalCoefficient(l, i) * weights_s[i] *
                BinominalCoefficient(m, j) * weights_t[j] *
                BinominalCoefficient(n, k) * weights_u[k] *
                cps[i + j * (l + 1) + k * (l + 1) * (m + 1)]);
      }
    }
  }

  return res;
}

void HalfedgeMesh::TransformFFD(const Vec3Vec& cps, unsigned l, unsigned m,
                                unsigned n) {
  Face_property<Vec3> f_normals = mesh_.get_face_property<Vec3>("f:normal");
  std::vector<Quaternion> normal_rotations(mesh_.halfedges_size());

  if (has_face_vertex_normals_) {
    Halfedge_property<Vec3> fv_normals =
        mesh_.get_halfedge_property<Vec3>("h:normal");
    for (Halfedge_iterator h_it = mesh_.halfedges_begin();
         h_it != mesh_.halfedges_end(); ++h_it) {
      Face f = mesh_.face(*h_it);
      if (mesh_.is_valid(f)) {
        normal_rotations[(*h_it).idx()] =
            Quaternion::FromTwoVectors(f_normals[f], fv_normals[*h_it]);
      }
    }
  }

  for (Vertex_iterator v_it = mesh_.vertices_begin();
       v_it != mesh_.vertices_end(); ++v_it) {
    mesh_.position(*v_it) =
        FFDInterpolation(cps, l, m, n, mesh_.position(*v_it));
  }

  for (Face_iterator f_it = mesh_.faces_begin(); f_it != mesh_.faces_end();
       ++f_it) {
    f_normals[*f_it] = mesh_.compute_face_normal(*f_it);
  }

  if (has_face_vertex_normals_) {
    Halfedge_property<Vec3> fv_normals =
        mesh_.get_halfedge_property<Vec3>("h:normal");
    for (Halfedge_iterator h_it = mesh_.halfedges_begin();
         h_it != mesh_.halfedges_end(); ++h_it) {
      Face f = mesh_.face(*h_it);
      if (mesh_.is_valid(f)) {
        fv_normals[*h_it] = normal_rotations[(*h_it).idx()] * f_normals[f];
      }
    }
  }

  unit_trafo_dirty_ = true;
}

const Vec3& HalfedgeMesh::UnitTrafoTranslation() const {
  if (unit_trafo_dirty_) {
    UpdateUnitTrafo();
  }
  return unit_trafo_translation_;
}

const Vec3& HalfedgeMesh::UnitTrafoScale() const {
  if (unit_trafo_dirty_) {
    UpdateUnitTrafo();
  }
  return unit_trafo_scale_;
}

void HalfedgeMesh::GetScaledUnitTrafo(const Vec3& scale, Isometry3* translation,
                                      Vec3* combined_scale) const {
  *combined_scale = scale.cwiseProduct(UnitTrafoScale());
  const Vec3 scale_trans = combined_scale->cwiseProduct(UnitTrafoTranslation());
  *translation = Isometry3::Identity();
  translation->translate(scale_trans);
}

Mat3 HalfedgeMesh::NormalMatrixFromScale(const Vec3& scale) const {
  bool is_degenerate = false;
  for (int i = 0; i < 3; ++i) {
    if (scale[i] == 0.0) {
      is_degenerate = true;
      break;
    }
  }

  Vec3 scale_inverse;
  if (!is_degenerate) {
    for (int i = 0; i < 3; ++i) {
      scale_inverse[i] = 1.0 / scale[i];
    }
  } else {
    for (int i = 0; i < 3; ++i) {
      if (scale[i] == 0.0) {
        scale_inverse[i] = 1.0;
      } else {
        scale_inverse[i] = 0.0;
      }
    }
  }

  return Eigen::Scaling(scale_inverse);
}

void HalfedgeMesh::UpdateUnitTrafo() const {
  AABB aabb;
  FillAABB(&aabb);

  Vec3 size = aabb.extent * 2.0;
  Vec3 scale;
  for (int i = 0; i < 3; ++i) {
    if (size[i] < EPSILON) {
      scale[i] = 0.0;
    } else {
      scale[i] = 1.0 / size[i];
    }
  }

  unit_trafo_translation_ = -(aabb.center - aabb.extent);
  unit_trafo_scale_ = scale;

  unit_trafo_dirty_ = false;
}

// TODO(stefalie): Is this a good hash? The idea is from:
// https://stackoverflow.com/questions/17016175/c-unordered-map-using-a-custom-class-type-as-the-key
std::size_t Vec3iHash::operator()(const Vec3i& vi) const {
  return (((std::hash<int>()(vi.x()) ^ (std::hash<int>()(vi.y()) << 1)) >> 1) ^
          (std::hash<int>()(vi.z()) << 1));
}

std::size_t Vec2iHash::operator()(const Vec2i& vi) const {
  return (std::hash<int>()(vi.x()) ^ (std::hash<int>()(vi.y()) << 1));
}

}  // namespace geometry

}  // namespace shapeml
