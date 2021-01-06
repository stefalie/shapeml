// Shape Modeling Language (ShapeML)
// Copyright (C) 2018  Stefan Lienhard
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

// This is an example test program that shows how to programatically create a
// (very simple) ShapeML grammar, how to execute it, and how to export the
// resulting meshes as an OBJ file.

#include <iostream>

#include "shapeml/exporter.h"
#include "shapeml/geometry/halfedge_mesh.h"
#include "shapeml/shape.h"

shapeml::Shape* MakeShape();

int main(int, char*[]) {
  shapeml::Shape* root = MakeShape();

  std::cout << "The shape tree is:\n";
  root->PrintToStreamRecursively(&std::cout);

  std::cout << "\nThe OBJ model is stored in test_shape_standalone.obj.\n";
  shapeml::Exporter(root, shapeml::ExportType::OBJ, "test_shape_standalone", "",
                    true);

  delete root;
  return 0;
}

// Note that this is not the most efficient way to arrive at the final model,
// but it best reflects what the interpreter actually does if you were to write
// the same as a grammar.
shapeml::Shape* MakeShape() {
  using shapeml::Shape;
  using shapeml::Vec3;
  using shapeml::Vec3Vec;
  using shapeml::Vec4;
  using shapeml::geometry::HalfedgeMesh;
  using shapeml::geometry::HalfedgeMeshPtr;

  // Axiom shape
  Shape* axiom = new Shape;
  axiom->set_name("Axiom");
  axiom->TranslateX(10.0);

  // Building lot shape
  Shape* bldg_lot = axiom->CreateOffspring();
  bldg_lot->set_name("Building Lot");
  bldg_lot->Size(Vec3(10.0, 0.0, 10.0));

  // Building lot mesh
  Vec3Vec vertices;
  vertices.emplace_back(Vec3(0.0, 0.0, 1.0));
  vertices.emplace_back(Vec3(1.0, 0.0, 1.0));
  vertices.emplace_back(Vec3(0.25, 0.0, 0.5));
  vertices.emplace_back(Vec3(1.0, 0.0, 0.0));
  vertices.emplace_back(Vec3(0.2, 0.0, 0.0));
  HalfedgeMeshPtr mesh_lot = std::make_shared<HalfedgeMesh>();
  mesh_lot->FromPolygon(vertices, nullptr, nullptr);

  bldg_lot->SetMeshIntoScope(mesh_lot);
  bldg_lot->AppendToParent();

  // Building mass model
  Shape* bldg_mass = bldg_lot->CreateOffspring();
  bldg_mass->set_name("Building Mass Model");
  HalfedgeMeshPtr mesh_mass = std::make_shared<HalfedgeMesh>(*bldg_lot->mesh());
  mesh_mass->TransformUnitTrafoAndScale(bldg_mass->Size());
  mesh_mass->ExtrudeAlongNormal(6.0);
  bldg_mass->SetMeshAndAdaptScope(mesh_mass);
  bldg_mass->AppendToParent();

  // Bottom terminal of the mass model
  Shape* bottom_terminal = bldg_mass->CreateOffspring();
  bottom_terminal->set_name("Building Mass Model Bottom");
  bottom_terminal->set_terminal(true);
  bottom_terminal->AppendToParent();

  // Roof terminal of the mass model
  Shape* roof = bldg_mass->CreateOffspring();
  roof->set_name("Building Roof");
  // I just happen to know that face 1 is always to top shape after an extrude.
  // TODO(stefalie): Expose face splits better in the mesh API, e.g.,
  // mesh->FaceSplit(MeshFace::TOP).
  HalfedgeMeshPtr mesh_roof = bldg_mass->mesh()->GetFaceComponent(1);
  mesh_roof->ExtrudeRoofHipOrGable(35.0, 0.5, false, 0.0);
  roof->SetMeshAndAdaptScope(mesh_roof);
  roof->SetColor(Vec4(0.7, 0.5, 0.3, 1.0));
  roof->set_terminal(true);
  roof->AppendToParent();

  return axiom;
}
