#pragma once

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include "cad_core/Shape.h"
#include "cad_feature/Feature.h"
#include "cad_sketch/Sketch.h"

namespace cad_ui {

class DocumentTree : public QTreeWidget {
    Q_OBJECT

public:
    explicit DocumentTree(QWidget* parent = nullptr);
    ~DocumentTree() = default;

    void AddShape(const cad_core::ShapePtr& shape);
    void RemoveShape(const cad_core::ShapePtr& shape);
    void AddFeature(const cad_feature::FeaturePtr& feature);
    void RemoveFeature(const cad_feature::FeaturePtr& feature);
    void Clear();
    void AddSketch(const std::shared_ptr<cad_sketch::Sketch>& sketch);
    void RemoveSketch(const std::shared_ptr<cad_sketch::Sketch>& sketch);

    // 获取当前树里所有的草图
    std::vector<std::shared_ptr<cad_sketch::Sketch>> GetAllSketches() const;
    // 控制草图节点加上/取消删除线
    void SetSketchUIHidden(const std::shared_ptr<cad_sketch::Sketch>& sketch, bool hidden);

signals:
    void ShapeSelected(const cad_core::ShapePtr& shape);
    void FeatureSelected(const cad_feature::FeaturePtr& feature);
    void ShapeDeleted(const cad_core::ShapePtr& shape);
    void ShapeVisibilityChanged(const cad_core::ShapePtr& shape, bool visible);
    void FeatureDeleted(const cad_feature::FeaturePtr& feature);
    void SketchDeleted(const std::shared_ptr<cad_sketch::Sketch>& sketch);
    void SketchVisibilityChanged(const std::shared_ptr<cad_sketch::Sketch>& sketch, bool visible);
    void SketchEditRequested(std::shared_ptr<cad_sketch::Sketch> sketch); 

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;


private slots:
    void OnItemClicked(QTreeWidgetItem* item, int column);
    void OnItemDoubleClicked(QTreeWidgetItem* item, int column);
    void OnDeleteItem();
    void OnRenameItem();
    void OnToggleVisibility();
    void OnEditSketch();

private:
    QTreeWidgetItem* m_shapesRoot;
    QTreeWidgetItem* m_featuresRoot;
    QMenu* m_contextMenu;
    QAction* m_deleteAction;
    QAction* m_renameAction;
    QAction* m_toggleVisibilityAction;
    QTreeWidgetItem* m_sketchesRoot;
    QAction* m_editSketchAction;

    void CreateContextMenu();
    void SetupTree();
};

} // namespace cad_ui