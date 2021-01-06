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

#include "shapeml/exporter.h"

#include <cmath>
#include <fstream>
#include <unordered_map>
#include <vector>

#include "shapeml/shape.h"

namespace shapeml {

Exporter::Exporter(const Shape* shape, ExportType type,
                   const std::string& file_name, const std::string& src_dir,
                   bool merge_vertices) {
  if (file_name.empty()) {
    std::cerr << "ERROR in exporter: Filename is empty.\n";
    return;
  }

  switch (type) {
    case ExportType::OBJ:
      ExportOBJ(shape, file_name, src_dir, merge_vertices);
      break;
  }
}

// For fixed floating point.
static inline int ffp(Scalar t) {
  assert(fabs(t * (1 << 16) + 0.5) <= INT_MAX);
  return static_cast<int>(t * (1 << 16) + 0.5);
}

void Exporter::ExportOBJ(const Shape* shape, const std::string& file_name,
                         const std::string& src_dir, bool merge_vertices) {
  LeafConstVisitor leaf_visitor;
  shape->AcceptVisitor(&leaf_visitor);

  Vec3Vec vertices;
  Vec3Vec normals;
  Vec2Vec uvs;
  IdxVecVec vertex_indices;
  IdxVecVec normal_indices;
  IdxVecVec uv_indices;
  std::vector<Material> materials;

  // Create a cache for normals, uv coordinates, and also for vertex position
  // (optional) so that duplicates can be eliminated in the mesh file.
  struct ExportBuffer {
    Vec3Vec vertices;
    Vec3Vec normals;
    Vec2Vec uvs;
    IdxVec vertex_indices;
    IdxVec normal_indices;
    IdxVec uv_indices;
  };
  std::unordered_map<geometry::HalfedgeMeshConstPtr, ExportBuffer>
      mesh_buffer_cache;

  // Map points to their indices in the merged arrays.
  std::unordered_map<Vec3i, int, geometry::Vec3iHash> vertex_to_idx;
  std::unordered_map<Vec3i, int, geometry::Vec3iHash> normal_to_idx;
  std::unordered_map<Vec2i, int, geometry::Vec2iHash> uv_to_idx;

  for (const Shape* s : leaf_visitor.leaves()) {
    if (s->terminal() && s->visible() && s->mesh()) {
      assert(!s->mesh()->Empty());

      const Affine3 trafo = s->MeshWorldTrafo();
      const Mat3 normal_matrix = s->MeshNormalMatrix();

      ExportBuffer* buf;
      auto it = mesh_buffer_cache.find(s->mesh());
      if (it == mesh_buffer_cache.end()) {
        auto jt = mesh_buffer_cache.emplace(s->mesh(), ExportBuffer());
        buf = &jt.first->second;
        s->mesh()->FillExportBuffers(&buf->vertices, &buf->normals, &buf->uvs,
                                     &buf->vertex_indices, &buf->normal_indices,
                                     &buf->uv_indices);
      } else {
        buf = &it->second;
      }

      // Map buffer indices to global indices.
      std::unordered_map<int, int> vertex_buf_idx_to_idx;
      std::unordered_map<int, int> normal_buf_idx_to_idx;
      std::unordered_map<int, int> uv_buf_idx_to_idx;

      int material_idx = -1;
      for (int i = 0; i < static_cast<int>(materials.size()); ++i) {
        if (materials[i] == s->material()) {
          material_idx = i;
          break;
        }
      }
      if (material_idx < 0) {
        material_idx = static_cast<int>(materials.size());
        materials.push_back(s->material());
        vertex_indices.push_back(IdxVec());
        normal_indices.push_back(IdxVec());
        uv_indices.push_back(IdxVec());
      }

      // vertices
      for (int i = 0; i < static_cast<int>(buf->vertices.size()); ++i) {
        const Vec3 v_trafo = trafo * buf->vertices[i];

        if (merge_vertices) {
          const Vec3i v_int(ffp(v_trafo.x()), ffp(v_trafo.y()),
                            ffp(v_trafo.z()));

          auto it =
              vertex_to_idx.emplace(v_int, static_cast<int>(vertices.size()));
          if (it.second) {
            vertices.push_back(v_trafo);
          }
          vertex_buf_idx_to_idx.emplace(i, it.first->second);
        } else {
          vertex_buf_idx_to_idx.emplace(i, static_cast<int>(vertices.size()));
          vertices.push_back(v_trafo);
        }
      }

      // normals
      for (int i = 0; i < static_cast<int>(buf->normals.size()); ++i) {
        const Vec3 n = buf->normals[i];
        assert(!std::isinf(n.x()) && !std::isinf(n.y()) && !std::isinf(n.z()));

        const Vec3 n_trafo = (normal_matrix * n).normalized();
        const Vec3i n_int(ffp(n_trafo.x()), ffp(n_trafo.y()), ffp(n_trafo.z()));

        auto it =
            normal_to_idx.emplace(n_int, static_cast<int>(normals.size()));
        if (it.second) {
          normals.push_back(n_trafo);
        }
        normal_buf_idx_to_idx.emplace(i, it.first->second);
      }

      // uvs
      for (int i = 0; i < static_cast<int>(buf->uvs.size()); ++i) {
        const Vec2 uv = buf->uvs[i];
        const Vec2i uv_int(ffp(uv.x()), ffp(uv.y()));

        auto it = uv_to_idx.emplace(uv_int, static_cast<int>(uvs.size()));
        if (it.second) {
          uvs.push_back(uv);
        }
        uv_buf_idx_to_idx.emplace(i, it.first->second);
      }

      assert(buf->vertex_indices.size() == buf->normal_indices.size());
      for (size_t i = 0; i < buf->vertex_indices.size(); ++i) {
        // +1 because OBJ indices start at 1.
        const unsigned vertex_idx =
            vertex_buf_idx_to_idx[buf->vertex_indices[i]] + 1;
        const unsigned normal_idx =
            normal_buf_idx_to_idx[buf->normal_indices[i]] + 1;
        vertex_indices[material_idx].push_back(vertex_idx);
        normal_indices[material_idx].push_back(normal_idx);

        if (!buf->uv_indices.empty()) {
          const unsigned uv_idx = uv_buf_idx_to_idx[buf->uv_indices[i]] + 1;
          uv_indices[material_idx].push_back(uv_idx);
        } else {
          // -1 indicates that is has no uv coordinate.
          uv_indices[material_idx].push_back(-1);
        }
      }
    }
  }

  // Short circuit exit if there are 0 faces.
  if (vertex_indices.empty()) {
    std::cerr << "ERROR in exporter: Didn't create file '" << file_name
              << ".obj' because the mesh has no faces.\n";
    return;
  }

  // Fix empty material names.
  for (size_t i = 0; i < materials.size(); ++i) {
    if (materials[i].name.empty()) {
      materials[i].name = "Material_" + std::to_string(i);
    }
  }

  // Fix duplicate material names. It's not the best way to avoid material name
  // conflicts, but it works.
  for (size_t i = 0; i < materials.size(); ++i) {
    for (size_t j = i + 1; j < materials.size(); ++j) {
      if (materials[i].name == materials[j].name) {
        materials[j].name += "_1";
      }
    }
  }

#ifdef _WIN32
  size_t slash = file_name.find_last_of("\\/");
#else
  size_t slash = file_name.rfind("/");
#endif
  const std::string export_dir = file_name.substr(0, slash + 1);
  const std::string mtllib_name = file_name.substr(slash + 1);

  using std::endl;
  {
    // Writing OBJ file.
    std::ostringstream oss;
    oss << "# ShapeML export\n";
    oss << "mtllib " << mtllib_name << ".mtl"
        << "\n\n";

    for (const Vec3& v : vertices) {
      oss << "v " << v.x() << " " << v.y() << " " << v.z() << '\n';
    }
    oss << '\n';
    for (const Vec3& n : normals) {
      oss << "vn " << n.x() << " " << n.y() << " " << n.z() << '\n';
    }
    oss << '\n';

    // Maya (2016) fails to import OBJ meshes that contain faces with and faces
    // without uv indices. A workaround is to make sure that all faces have uv
    // indices. For that one can add a dummy uv coordinate.
    // TODO(stefalie): Consider removing this, but not before having verified
    // that Maya fixed this in newer versions.
    const bool maya_hack = false;

    for (const Vec2& uv : uvs) {
      oss << "vt " << uv.x() << " " << uv.y() << '\n';
    }
    oss << '\n';

    for (size_t i = 0; i < vertex_indices.size(); ++i) {
      oss << "usemtl " << materials[i].name << '\n';
      for (size_t j = 0; j < vertex_indices[i].size(); j += 3) {
        // Somtimes, due to the fixed point rounding, it can happend that we get
        // degenerate triangles with two (or even) three idetical vertex
        // indices. We simply skip such triangles.
        // TODO(stefalie): Ideally we should check if the vertices of a skippped
        // triangle are used elsewhere, and if not we should probably delete it.
        if (!(vertex_indices[i][j] != vertex_indices[i][j + 1] &&
              vertex_indices[i][j] != vertex_indices[i][j + 2] &&
              vertex_indices[i][j + 1] != vertex_indices[i][j + 2])) {
          continue;
        }

        if (maya_hack && !uvs.empty() && uv_indices[i][j] == (unsigned)-1) {
          // Faces without uv coordinates for Maya iff any of the faces have uv
          // coordinates.
          assert(uv_indices[i][j + 1] == (unsigned)-1);
          assert(uv_indices[i][j + 2] == (unsigned)-1);
          oss << 'f';
          oss << ' ' << vertex_indices[i][j] << '/' << uvs.size() << '/'
              << normal_indices[i][j];
          oss << ' ' << vertex_indices[i][j + 1] << '/' << uvs.size() << '/'
              << normal_indices[i][j + 1];
          oss << ' ' << vertex_indices[i][j + 2] << '/' << uvs.size() << '/'
              << normal_indices[i][j + 2];
          oss << '\n';
        } else if (uv_indices[i][j] == (unsigned)-1) {
          // Faces without uv coordinates.
          assert(uv_indices[i][j + 1] == (unsigned)-1);
          assert(uv_indices[i][j + 2] == (unsigned)-1);
          oss << 'f';
          oss << ' ' << vertex_indices[i][j] << "//" << normal_indices[i][j];
          oss << ' ' << vertex_indices[i][j + 1] << "//"
              << normal_indices[i][j + 1];
          oss << ' ' << vertex_indices[i][j + 2] << "//"
              << normal_indices[i][j + 2];
          oss << '\n';
        } else {
          // Faces with uv coordinates.
          oss << 'f';
          oss << ' ' << vertex_indices[i][j] << '/' << uv_indices[i][j] << '/'
              << normal_indices[i][j];
          oss << ' ' << vertex_indices[i][j + 1] << '/' << uv_indices[i][j + 1]
              << '/' << normal_indices[i][j + 1];
          oss << ' ' << vertex_indices[i][j + 2] << '/' << uv_indices[i][j + 2]
              << '/' << normal_indices[i][j + 2];
          oss << '\n';
        }
      }
    }

    std::ofstream ofs(file_name + ".obj");
    if (ofs.good()) {
      ofs << oss.str();
      ofs.close();
    } else {
      std::cerr << "ERROR in exporter: Couldn't create file '" << file_name
                << ".obj'.\n";
    }
  }

  {
    // Writing MTL file.
    std::ostringstream oss;
    for (size_t i = 0; i < materials.size(); ++i) {
      const Material& m = materials[i];
      oss << "newmtl " << m.name << '\n';
      oss << "Kd " << m.color[0] << " " << m.color[1] << " " << m.color[2]
          << '\n';

      // Horrible specularity conversion from PBR metallic/roughness to Phong.
      // TODO(stefalie): Find a good way to handle this, if there is one.
      const Vec3 specular_color = m.color.head<3>() * m.metallic;
      oss << "Ks " << specular_color[0] << " " << specular_color[1] << " "
          << specular_color[2] << '\n';
      oss << "Ns " << 1000.0f * m.roughness << '\n';

      if (m.color[3] < 1.0) {
        oss << "d " << m.color[3] << '\n';
        oss << "Tr " << (1.0 - m.color[3]) << '\n';
      }

      if (!m.texture.empty()) {
        // Copy texture file to output location.
#ifdef _WIN32
        size_t slash = m.texture.find_last_of("\\/");
#else
        size_t slash = m.texture.rfind("/");
#endif
        const std::string texture_file = m.texture.substr(slash + 1);
        std::ifstream src(src_dir + m.texture, std::ios::binary);
        std::ofstream dst(export_dir + texture_file, std::ios::binary);
        if (src.good() && dst.good()) {
          dst << src.rdbuf();
        } else {
          std::cerr << "ERROR in exporter: Couldn't copy texture file '";
          std::cerr << (src_dir + m.texture) << "' to '"
                    << (export_dir + texture_file);
          std::cerr << "'.\n";
        }

        oss << "map_Kd " << texture_file << '\n';
      }
    }

    std::ofstream ofs(file_name + ".mtl");
    if (ofs.good()) {
      ofs << oss.str();
      ofs.close();
    } else {
      std::cerr << "ERROR in exporter: Couldn't create material file '"
                << file_name << ".mtl'.\n";
    }
  }
}

}  // namespace shapeml
