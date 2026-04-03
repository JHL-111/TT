#include "cad_feature/LoftFeature.h"
#include <BRepOffsetAPI_ThruSections.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>

namespace cad_feature {

    LoftFeature::LoftFeature() : Feature(FeatureType::Loft, "Loft") {
        SetParameter("solid", 1.0); // 默认生成实体 (Solid)
    }

    LoftFeature::LoftFeature(const std::string& name) : Feature(FeatureType::Loft, name) {
        SetParameter("solid", 1.0);
    }

    void LoftFeature::AddSection(const cad_core::ShapePtr& section) {
        if (section) {
            m_sections.push_back(section);
        }
    }

    void LoftFeature::ClearSections() {
        m_sections.clear();
    }

    const std::vector<cad_core::ShapePtr>& LoftFeature::GetSections() const {
        return m_sections;
    }

    int LoftFeature::GetSectionCount() const {
        return static_cast<int>(m_sections.size());
    }

    void LoftFeature::SetSolid(bool solid) {
        SetParameter("solid", solid ? 1.0 : 0.0);
    }

    bool LoftFeature::GetSolid() const {
        return GetParameter("solid") != 0.0;
    }

    cad_core::ShapePtr LoftFeature::CreateShape() const {
        if (!ValidateParameters()) {
            return nullptr;
        }
        return LoftSections();
    }

    bool LoftFeature::ValidateParameters() const {
        return GetSectionCount() >= 2 && AreSectionsValid();
    }

    std::shared_ptr<cad_core::ICommand> LoftFeature::CreateCommand() const {
        return nullptr; // 如果目前没有 Command 模式需求，可直接返回 nullptr
    }

    bool LoftFeature::AreSectionsValid() const {
        for (const auto& section : m_sections) {
            if (!section || section->GetOCCTShape().IsNull()) {
                return false;
            }
        }
        return true;
    }

    // 核心放样算法生成 (Core Loft Algorithm)
    cad_core::ShapePtr LoftFeature::LoftSections() const {
        try {
            // 初始化 OCCT 的放样生成器，参数为 (是否生成实体，是否为直纹面)
            BRepOffsetAPI_ThruSections loftMaker(GetSolid(), false);

            for (const auto& section : m_sections) {
                TopoDS_Shape occShape = section->GetOCCTShape();
                TopoDS_Wire sectionWire;

                // 提取线框 (Wire Extraction)
                if (occShape.ShapeType() == TopAbs_WIRE) {
                    sectionWire = TopoDS::Wire(occShape);
                }
                // 如果选中了面 (Face)，则提取它的外围边界线框
                else if (occShape.ShapeType() == TopAbs_FACE) {
                    TopExp_Explorer exp(occShape, TopAbs_WIRE);
                    if (exp.More()) {
                        sectionWire = TopoDS::Wire(exp.Current());
                    }
                }

                if (!sectionWire.IsNull()) {
                    loftMaker.AddWire(sectionWire); // 将轮廓按顺序加入
                }
                else {
                    return nullptr;
                }
            }

            loftMaker.Build();

            if (loftMaker.IsDone()) {
                return std::make_shared<cad_core::Shape>(loftMaker.Shape());
            }
        }
        catch (...) {
            return nullptr;
        }
        return nullptr;
    }

} // namespace cad_feature