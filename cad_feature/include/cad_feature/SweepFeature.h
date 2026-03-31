#pragma once

#include "Feature.h"
#include "cad_sketch/Sketch.h"
#include "cad_core/Shape.h"
#include <vector>

namespace cad_feature {

class SweepFeature : public Feature {
public:
    SweepFeature();
    SweepFeature(const std::string& name);
    virtual ~SweepFeature() = default;

    // Profile and path operations
    void SetProfileShape(const cad_core::ShapePtr& profile);
    const cad_core::ShapePtr& GetProfileShape() const;
    
    void SetPathShape(const cad_core::ShapePtr& path);
    const cad_core::ShapePtr& GetPathShape() const;
    
    // Sweep parameters
    void SetTwistAngle(double angle);
    double GetTwistAngle() const;
    
    void SetScaleFactor(double factor);
    double GetScaleFactor() const;
    
    void SetKeepOriginalOrientation(bool keep);
    bool GetKeepOriginalOrientation() const;
    
    // Feature interface
    cad_core::ShapePtr CreateShape() const override;
    bool ValidateParameters() const override;
    std::shared_ptr<cad_core::ICommand> CreateCommand() const override;

private:
    cad_core::ShapePtr m_profileShape;
    cad_core::ShapePtr m_pathShape;

    bool IsProfileValid() const;
    bool IsPathValid() const;
    cad_core::ShapePtr SweepProfile() const;
};

using SweepFeaturePtr = std::shared_ptr<SweepFeature>;

} // namespace cad_feature