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

#include <cassert>

#include "shapeml/evaluator.h"
#include "shapeml/interpreter.h"
#include "shapeml/shape.h"

namespace shapeml {

template <>
const char* ShapeAttributeEvaluator::type_name_ = "shape attribute";

template class Evaluator<AttrEvalContext>;

// The rest of this file contains the evaluators for all built-in shape
// attributes.

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_size_x, "size_x", 0, {});
struct AttrEval_size_x : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->SizeX());
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_size_y, "size_y", 0, {});
struct AttrEval_size_y : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->SizeY());
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_size_z, "size_z", 0, {});
struct AttrEval_size_z : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->SizeZ());
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_pos_x, "pos_x", 0, {});
struct AttrEval_pos_x : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->PositionX());
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_pos_y, "pos_y", 0, {});
struct AttrEval_pos_y : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->PositionY());
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_pos_z, "pos_z", 0, {});
struct AttrEval_pos_z : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->PositionZ());
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_pos_world_x, "pos_world_x", 0, {});
struct AttrEval_pos_world_x : ShapeAttributeEvaluator {
  bool Eval() final {
    const Value ret(context_->shape->WorldTrafo().translation().x());
    context_->return_value = ret;
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_pos_world_y, "pos_world_y", 0, {});
struct AttrEval_pos_world_y : ShapeAttributeEvaluator {
  bool Eval() final {
    const Value ret(context_->shape->WorldTrafo().translation().y());
    context_->return_value = ret;
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_pos_world_z, "pos_world_z", 0, {});
struct AttrEval_pos_world_z : ShapeAttributeEvaluator {
  bool Eval() final {
    const Value ret(context_->shape->WorldTrafo().translation().z());
    context_->return_value = ret;
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_area, "area", 0, {});
struct AttrEval_area : ShapeAttributeEvaluator {
  bool Eval() final {
    Scalar area = 0.0;

    if (CheckNonEmptyMesh(context_->shape) ||
        CheckSingleFaceMesh(context_->shape)) {
      return true;
    }

    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*context_->shape->mesh());
    tmp->TransformUnitTrafoAndScale(context_->shape->Size());
    area = tmp->GetFaceArea(0);

    context_->return_value = Value(area);
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_color_r, "color_r", 0, {});
struct AttrEval_color_r : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->material().color[0]);
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_color_g, "color_g", 0, {});
struct AttrEval_color_g : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->material().color[1]);
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_color_b, "color_b", 0, {});
struct AttrEval_color_b : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->material().color[2]);
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_color_a, "color_a", 0, {});
struct AttrEval_color_a : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->material().color[3]);
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_metallic, "metallic", 0, {});
struct AttrEval_metallic : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->material().metallic);
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_roughness, "roughness", 0, {});
struct AttrEval_roughness : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->material().roughness);
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_reflectance, "reflectance", 0, {});
struct AttrEval_reflectance : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->material().reflectance);
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_texture, "texture", 0, {});
struct AttrEval_texture : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->material().texture);
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_material_name, "material_name", 0, {});
struct AttrEval_material_name : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->material().name);
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_index, "index", 0, {});
struct AttrEval_index : ShapeAttributeEvaluator {
  bool Eval() final {
    if (context_->shape->index() < 0) {
      error_message_ = "Built-in shape attribute 'index' has not been set yet.";
      return true;
    }
    context_->return_value = Value(context_->shape->index());
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_visible, "visible", 0, {});
struct AttrEval_visible : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->visible());
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_depth, "depth", 0, {});
struct AttrEval_depth : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->Depth());
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_label, "label", 0, {});
struct AttrEval_label : ShapeAttributeEvaluator {
  bool Eval() final {
    context_->return_value = Value(context_->shape->name());
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_get, "get", 1, {ValueType::STRING});
struct AttrEval_get : ShapeAttributeEvaluator {
  bool ValidateSpecial() final {
    const std::string& attr_name = *args_->at(0).s;

    bool attr_name_invalid = false;
    if (attr_name.empty() || !(isalpha(attr_name[0]) || attr_name[0] == '_')) {
      attr_name_invalid = true;
    }
    for (size_t i = 1; i < attr_name.size(); ++i) {
      if (!(isalpha(attr_name[0]) || isdigit(attr_name[0]) ||
            attr_name[0] == '_')) {
        attr_name_invalid = true;
        break;
      }
    }

    if (attr_name_invalid) {
      std::ostringstream oss;
      oss << "Parameter 1 for shape attribute 'get' is not a valid name for a ";
      oss << "custom shape attribute.";
      oss << "(Provided name: " << args_->at(0) << ".)";
      error_message_ = oss.str();
      return true;
    }
    return false;
  }

  bool Eval() final {
    Value val(0);
    if (!context_->interpreter->GetShapeStackTop()->GetCustomAttribute(
            *args_->at(0).s, &val)) {
      std::ostringstream oss;
      oss << "Parameter 1 for shape attribute 'get' is not the name of any of ";
      oss << "the custom shape attributes.";
      oss << "(Provided name: " << args_->at(0) << ".)";
      error_message_ = oss.str();
      return true;
    }
    context_->return_value = val;
    return false;
  }
};

REGISTER_ATTRIBUTE_EVALUATOR(AttrEval_occlusion, "occlusion", -1, {});
struct AttrEval_occlusion : ShapeAttributeEvaluator {
  bool ValidateSpecial() final {
    if (CheckArgNumber({0, 1})) {
      return true;
    }
    if (args_->size() == 1) {
      ValidateType(ValueType::STRING, 0);

      if (args_->at(0).s->empty()) {
        std::ostringstream oss;
        oss << "Parameter 1 for shape attribute 'occlusion' to cannot be an ";
        oss << "empty string.";
        error_message_ = oss.str();
        return true;
      }
    }
    return false;
  }

  bool Eval() final {
    context_->return_value = Value("none");
    if (!context_->interpreter->octree()) {
      return false;
    }

    const Shape* shape = context_->interpreter->GetShapeStackTop();

    std::vector<geometry::OctreeElement*> candidates;
    const geometry::AABB aabb = shape->GetWorldSpaceAABB();
    context_->interpreter->octree()->IntersectWith(aabb, &candidates);

    // Slightly shorter OBB for thresholding. This helps to get a 'full' return
    // value for two almost identical scopes.
    geometry::OBB thres_obb = shape->GetWorldSpaceOBB();
    thres_obb.extent *= 0.999;

    for (const geometry::OctreeElement* ol : candidates) {
      const OcclusionShape* occ_shape = static_cast<const OcclusionShape*>(ol);
      if (shape->IsAnchestor(occ_shape->parent())) {
        continue;
      }

      // Continue if the label that the query asks for does not correspond to
      // the label of the candidate shape.
      if (args_->size() == 1 && occ_shape->name() != *args_->at(0).s) {
        continue;
      }

      if (occ_shape->obb().Contains(thres_obb)) {
        context_->return_value = Value("full");
        return false;
      }

      if (occ_shape->obb().Intersect(thres_obb)) {
        context_->return_value = Value("partial");
      }
    }

    return false;
  }
};

}  // namespace shapeml
