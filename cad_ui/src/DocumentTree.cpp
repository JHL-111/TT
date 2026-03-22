#include "cad_ui/DocumentTree.h"
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>

Q_DECLARE_METATYPE(cad_core::ShapePtr)
Q_DECLARE_METATYPE(cad_feature::FeaturePtr)
Q_DECLARE_METATYPE(std::shared_ptr<cad_sketch::Sketch>)

namespace cad_ui {

    DocumentTree::DocumentTree(QWidget* parent) : QTreeWidget(parent) {
        SetupTree();
        CreateContextMenu();

        connect(this, &QTreeWidget::itemClicked, this, &DocumentTree::OnItemClicked);
        connect(this, &QTreeWidget::itemDoubleClicked, this, &DocumentTree::OnItemDoubleClicked);
    }

    void DocumentTree::SetupTree() {
        setHeaderLabel("Document");
        setRootIsDecorated(true);
        setSelectionMode(QAbstractItemView::SingleSelection);

        // Create root items
        m_shapesRoot = new QTreeWidgetItem(this);
        m_shapesRoot->setText(0, "Shapes");
        m_shapesRoot->setExpanded(true);

        m_sketchesRoot = new QTreeWidgetItem(this);
        m_sketchesRoot->setText(0, "Sketches");
        m_sketchesRoot->setExpanded(true);

        m_featuresRoot = new QTreeWidgetItem(this);
        m_featuresRoot->setText(0, "Features");
        m_featuresRoot->setExpanded(true);

        addTopLevelItem(m_shapesRoot);
        addTopLevelItem(m_sketchesRoot);
        addTopLevelItem(m_featuresRoot);
    }

    void DocumentTree::CreateContextMenu() {
        m_contextMenu = new QMenu(this);

        // 编辑动作
        m_editSketchAction = new QAction("Edit Sketch", this);
        m_deleteAction = new QAction("Delete", this);
        m_renameAction = new QAction("Rename", this);
        m_toggleVisibilityAction = new QAction("Toggle Visibility", this);

        // 把编辑按钮也加进原有的菜单里
        m_contextMenu->addAction(m_editSketchAction);
        m_contextMenu->addSeparator(); // 加条分割线好看点
        m_contextMenu->addAction(m_deleteAction);
        m_contextMenu->addAction(m_renameAction);
        m_contextMenu->addSeparator();
        m_contextMenu->addAction(m_toggleVisibilityAction);

        // 绑定信号
        connect(m_editSketchAction, &QAction::triggered, this, &DocumentTree::OnEditSketch);
        connect(m_deleteAction, &QAction::triggered, this, &DocumentTree::OnDeleteItem);
        connect(m_renameAction, &QAction::triggered, this, &DocumentTree::OnRenameItem);
        connect(m_toggleVisibilityAction, &QAction::triggered, this, &DocumentTree::OnToggleVisibility);
    }

    // 整合后的右键弹出事件
    void DocumentTree::contextMenuEvent(QContextMenuEvent* event) {
        QTreeWidgetItem* item = itemAt(event->pos());

        // 如果点在了空白处，或者根节点上，不弹菜单
        if (!item || item == m_shapesRoot || item == m_featuresRoot || item == m_sketchesRoot) {
            return;
        }

        // 检查当前点中的是不是草图
        auto sketch = item->data(0, Qt::UserRole).value<std::shared_ptr<cad_sketch::Sketch>>();

        // 只有点中草图时，“编辑草图”按钮才可见，否则隐藏
        m_editSketchAction->setVisible(sketch != nullptr);

        // 弹出你原有的、功能完整的菜单
        m_contextMenu->exec(event->globalPos());
    }

    // 向外发射编辑信号
    void DocumentTree::OnEditSketch() {
        QTreeWidgetItem* item = currentItem();
        if (!item) return;

        auto sketch = item->data(0, Qt::UserRole).value<std::shared_ptr<cad_sketch::Sketch>>();
        if (sketch) {
            emit SketchEditRequested(sketch);
        }
    }

    void DocumentTree::AddShape(const cad_core::ShapePtr& shape) {
        if (!shape) return;

        QTreeWidgetItem* item = new QTreeWidgetItem(m_shapesRoot);
        item->setText(0, QString("Shape %0").arg(m_shapesRoot->childCount()));
        item->setData(0, Qt::UserRole, QVariant::fromValue(shape));

        m_shapesRoot->addChild(item);
        m_shapesRoot->setExpanded(true);
    }

    void DocumentTree::RemoveShape(const cad_core::ShapePtr& shape) {
        if (!shape) return;

        for (int i = 0; i < m_shapesRoot->childCount(); ++i) {
            QTreeWidgetItem* item = m_shapesRoot->child(i);
            auto itemShape = item->data(0, Qt::UserRole).value<cad_core::ShapePtr>();
            if (itemShape == shape) {
                m_shapesRoot->removeChild(item);
                delete item;
                break;
            }
        }
    }

    void DocumentTree::AddSketch(const std::shared_ptr<cad_sketch::Sketch>& sketch) {
        if (!sketch) return;

        // 查重拦截：如果这棵树上已经存在这个草图了，就直接退出
        for (int i = 0; i < m_sketchesRoot->childCount(); ++i) {
            QTreeWidgetItem* existingItem = m_sketchesRoot->child(i);
            auto existingSketch = existingItem->data(0, Qt::UserRole).value<std::shared_ptr<cad_sketch::Sketch>>();
            if (existingSketch == sketch) {
                return; // 发现重复，直接拦截
            }
        }

        // 先获取当前的草图数量 (此时还没有添加新节点)
        int currentCount = m_sketchesRoot->childCount();

        QTreeWidgetItem* item = new QTreeWidgetItem();

        // 设置名字
        item->setText(0, QString("Sketch %1").arg(currentCount + 1));
        item->setData(0, Qt::UserRole, QVariant::fromValue(sketch));

        // 属性都设置好之后，最后再将它挂载到树上
        m_sketchesRoot->addChild(item);
        m_sketchesRoot->setExpanded(true);
    }

    void DocumentTree::RemoveSketch(const std::shared_ptr<cad_sketch::Sketch>& sketch) {
        if (!sketch || !m_sketchesRoot) return;

        for (int i = 0; i < m_sketchesRoot->childCount(); ++i) {
            QTreeWidgetItem* item = m_sketchesRoot->child(i);
            auto itemSketch = item->data(0, Qt::UserRole).value<std::shared_ptr<cad_sketch::Sketch>>();
            if (itemSketch == sketch) {
                m_sketchesRoot->removeChild(item);
                delete item;
                break;
            }
        }
    }

    void DocumentTree::AddFeature(const cad_feature::FeaturePtr& feature) {
        if (!feature) return;

        QTreeWidgetItem* item = new QTreeWidgetItem(m_featuresRoot);
        item->setText(0, QString::fromStdString(feature->GetName()));
        item->setData(0, Qt::UserRole, QVariant::fromValue(feature));

        m_featuresRoot->addChild(item);
        m_featuresRoot->setExpanded(true);
    }

    void DocumentTree::RemoveFeature(const cad_feature::FeaturePtr& feature) {
        if (!feature) return;

        for (int i = 0; i < m_featuresRoot->childCount(); ++i) {
            QTreeWidgetItem* item = m_featuresRoot->child(i);
            auto itemFeature = item->data(0, Qt::UserRole).value<cad_feature::FeaturePtr>();
            if (itemFeature == feature) {
                m_featuresRoot->removeChild(item);
                delete item;
                break;
            }
        }
    }


    void DocumentTree::Clear() {
        m_shapesRoot->takeChildren();
        m_featuresRoot->takeChildren();
    }


    void DocumentTree::OnItemClicked(QTreeWidgetItem* item, int column) {
        Q_UNUSED(column);

        if (!item || item == m_shapesRoot || item == m_featuresRoot) {
            return;
        }

        // Check if it's a shape
        auto shape = item->data(0, Qt::UserRole).value<cad_core::ShapePtr>();
        if (shape) {
            emit ShapeSelected(shape);
            return;
        }

        // Check if it's a feature
        auto feature = item->data(0, Qt::UserRole).value<cad_feature::FeaturePtr>();
        if (feature) {
            emit FeatureSelected(feature);
            return;
        }
    }

    void DocumentTree::OnItemDoubleClicked(QTreeWidgetItem* item, int column) {
        Q_UNUSED(column);

        if (item && item != m_shapesRoot && item != m_featuresRoot) {
            // Start editing the item name
            editItem(item, 0);
        }
    }

    void DocumentTree::OnDeleteItem() {
        QTreeWidgetItem* item = currentItem();
        if (!item || item == m_shapesRoot || item == m_featuresRoot || item == m_sketchesRoot) {
            return;
        }

        auto shape = item->data(0, Qt::UserRole).value<cad_core::ShapePtr>();
        if (shape) {
            emit ShapeDeleted(shape); // 通知外界去删除真实的 3D 数据
        }

        auto feature = item->data(0, Qt::UserRole).value<cad_feature::FeaturePtr>();
        if (feature) {
            emit FeatureDeleted(feature); // 通知外界去删除真实的特征数据
        }

        auto sketch = item->data(0, Qt::UserRole).value<std::shared_ptr<cad_sketch::Sketch>>();
        if (sketch) {
            emit SketchDeleted(sketch); // 通知外界删除草图数据与视图对象
        }


        // Remove the item
        if (item->parent() == m_shapesRoot) {
            m_shapesRoot->removeChild(item);
        }
        else if (item->parent() == m_featuresRoot) {
            m_featuresRoot->removeChild(item);
        }

        delete item;
    }

    void DocumentTree::OnRenameItem() {
        QTreeWidgetItem* item = currentItem();
        if (item && item != m_shapesRoot && item != m_featuresRoot && item != m_sketchesRoot) {
            editItem(item, 0);
        }
    }

    void DocumentTree::OnToggleVisibility() {
        QTreeWidgetItem* item = currentItem();
        if (!item || item == m_shapesRoot || item == m_featuresRoot) {
            return;
        }

        // 切换字体删除线样式（作为UI表现）
        QFont font = item->font(0);
        bool isCurrentlyHidden = font.strikeOut();
        font.setStrikeOut(!isCurrentlyHidden); // 状态反转
        item->setFont(0, font);

        auto shape = item->data(0, Qt::UserRole).value<cad_core::ShapePtr>();
        if (shape) {
            // 如果当前没有删除线，说明下一步是隐藏(false)；如果有，下一步是显示(true)
            bool willBeVisible = isCurrentlyHidden;
            emit ShapeVisibilityChanged(shape, willBeVisible);
        }

        // 如果是草图，则发射草图的隐藏/显示信号
        auto sketch = item->data(0, Qt::UserRole).value<std::shared_ptr<cad_sketch::Sketch>>();
        if (sketch) {
            bool willBeVisible = isCurrentlyHidden;
            emit SketchVisibilityChanged(sketch, willBeVisible);
        }
    }

    std::vector<std::shared_ptr<cad_sketch::Sketch>> DocumentTree::GetAllSketches() const {
        std::vector<std::shared_ptr<cad_sketch::Sketch>> sketches;
        if (!m_sketchesRoot) return sketches;

        // 遍历 Sketches 根节点下的所有子节点
        for (int i = 0; i < m_sketchesRoot->childCount(); ++i) {
            QTreeWidgetItem* item = m_sketchesRoot->child(i);
            auto sketch = item->data(0, Qt::UserRole).value<std::shared_ptr<cad_sketch::Sketch>>();
            if (sketch) {
                sketches.push_back(sketch);
            }
        }
        return sketches;
    }

    void DocumentTree::SetSketchUIHidden(const std::shared_ptr<cad_sketch::Sketch>& sketch, bool hidden) {
        if (!m_sketchesRoot || !sketch) return;

        for (int i = 0; i < m_sketchesRoot->childCount(); ++i) {
            QTreeWidgetItem* item = m_sketchesRoot->child(i);
            auto itemSketch = item->data(0, Qt::UserRole).value<std::shared_ptr<cad_sketch::Sketch>>();
            if (itemSketch == sketch) {
                QFont font = item->font(0);
                font.setStrikeOut(hidden); // 加上或取消删除线
                item->setFont(0, font);
                break;
            }
        }
    }

}// namespace cad_ui

#include "DocumentTree.moc"