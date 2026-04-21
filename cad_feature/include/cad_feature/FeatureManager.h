#pragma once

#include "Feature.h"
#include <vector>
#include <memory>
#include <string>

namespace cad_feature {

    class FeatureManager {
    public:
        FeatureManager();
        ~FeatureManager() = default;

        // Feature management
        void AddFeature(const FeaturePtr& feature);
        void RemoveFeature(const FeaturePtr& feature);
        void ClearFeatures();

        const std::vector<FeaturePtr>& GetFeatures() const;
        FeaturePtr GetFeatureById(int id) const;
        FeaturePtr GetFeatureByName(const std::string& name) const;

        // Feature operations
        bool ExecuteFeature(const FeaturePtr& feature);
        bool ExecuteAllFeatures();

        void SetFeatureActive(const FeaturePtr& feature, bool active);
        void SetAllFeaturesActive(bool active);

        // Feature ordering
        void MoveFeatureUp(const FeaturePtr& feature);
        void MoveFeatureDown(const FeaturePtr& feature);
        void MoveFeatureToIndex(const FeaturePtr& feature, int index);

        // Update and rebuild
        void UpdateFeature(const FeaturePtr& feature);
        void RebuildAllFeatures();

        // Utility methods
        int GetFeatureCount() const;
        bool IsEmpty() const;

        // Events (used to notify the UI)
        void SetFeatureAddedCallback(std::function<void(const FeaturePtr&)> callback);
        void SetFeatureRemovedCallback(std::function<void(const FeaturePtr&)> callback);
        void SetFeatureUpdatedCallback(std::function<void(const FeaturePtr&)> callback);

    private:
        std::vector<FeaturePtr> m_features;

        // Callback functions
        std::function<void(const FeaturePtr&)> m_featureAddedCallback;
        std::function<void(const FeaturePtr&)> m_featureRemovedCallback;
        std::function<void(const FeaturePtr&)> m_featureUpdatedCallback;

        int FindFeatureIndex(const FeaturePtr& feature) const;
        void NotifyFeatureAdded(const FeaturePtr& feature);
        void NotifyFeatureRemoved(const FeaturePtr& feature);
        void NotifyFeatureUpdated(const FeaturePtr& feature);
    };

} // namespace cad_feature