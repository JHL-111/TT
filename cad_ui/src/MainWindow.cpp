#include "cad_ui/MainWindow.h"
#include "cad_ui/ExportDialog.h"
#include "cad_ui/AboutDialog.h"
#include "cad_ui/CreatePrimitiveDialog.h"
#include "cad_core/CreateBoxCommand.h"
#include "cad_core/CreateCylinderCommand.h"
#include "cad_core/CreateSphereCommand.h"
#include "cad_core/OCAFManager.h"
#include "cad_core/ShapeFactory.h"
#include "cad_core/BooleanOperations.h"
#include "cad_core/FilletChamferOperations.h"
#include "cad_core/SelectionManager.h"
#include "cad_ui/CreateExtrudeDialog.h"
#include "cad_feature/ExtrudeFeature.h"
#include "cad_sketch/SketchProfile.h"
#include "cad_feature/BooleanFeature.h"
#include "cad_feature/FilletChamferFeature.h"
#include "cad_feature/RectangularFaceFeature.h"
#include "cad_ui/CreateSweepDialog.h"
#include "cad_feature/SweepFeature.h"
#include "cad_feature/LoftFeature.h"
#include "cad_feature/RevolveFeature.h"
#include "cad_core/OCAFTransactionCommand.h"

#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Tool.hxx>
#include <GeomLProp_SLProps.hxx>
#include <BRepTools.hxx>
#include <TopoDS.hxx>
#include <iostream>
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QCloseEvent>
#include <QSplitter>
#include <QSettings>
#include <QTabWidget>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QLabel>
#include <map>

namespace cad_ui {

    MainWindow::MainWindow(QWidget* parent)
        : QMainWindow(parent), m_tabWidget(nullptr), m_documentModified(false), m_currentBooleanDialog(nullptr), m_currentFilletChamferDialog(nullptr),
        m_currentTransformDialog(nullptr), m_previewActive(false),
        m_waitingForFaceSelection(false) {


        // Initialize managers
        m_commandManager = std::make_unique<cad_core::CommandManager>();
        m_ocafManager = std::make_unique<cad_core::OCAFManager>();
        m_featureManager = std::make_unique<cad_feature::FeatureManager>();

        // Create UI components
        CreateActions();
        CreateMenus();

        // Create selection mode combo box before toolbars
        CreateSelectionModeCombo();

        CreateToolBars();
        CreateStatusBar();
        CreateDockWidgets();
        //CreateTitleBar();
        CreateConsole();

        // Create multi-document tab interface
        m_tabWidget = new QTabWidget(this);
        m_tabWidget->setTabsClosable(true);
        m_tabWidget->setMovable(true);
        m_tabWidget->setObjectName("documentTabs");


        // Create first document tab
        m_viewer = new QtOccView(this);
        m_viewer->setObjectName("viewer3D");
        m_tabWidget->addTab(m_viewer, "Document 1");

        // Create main splitter with viewer and console
        m_mainSplitter = new QSplitter(Qt::Vertical, this);
        m_mainSplitter->addWidget(m_tabWidget);
        m_mainSplitter->addWidget(m_console);
        m_mainSplitter->setStretchFactor(0, 3); // Give viewer more space
        m_mainSplitter->setStretchFactor(1, 1); // Console gets less space

        setCentralWidget(m_mainSplitter);

        // Initialize theme manager
        m_themeManager = new ThemeManager(this);

        // Initialize dialog pointers to null
        m_currentBooleanDialog = nullptr;
        m_currentFilletChamferDialog = nullptr;

        // Connect signals
        ConnectSignals();

        // Connect tab widget signals
        connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::CloseDocumentTab);
        connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::OnTabChanged);

        // Set window properties - frameless window
        setWindowTitle("JLi CAD");
        setMinimumSize(800, 600);
        resize(1200, 800);


        // Update UI
        UpdateActions();
        UpdateWindowTitle();
    }

    bool MainWindow::Initialize() {

        // Initialize OCAF manager
        if (!m_ocafManager->Initialize()) {
            QMessageBox::critical(this, "Error", "Failed to initialize OCAF document manager");
            return false;
        }

        // Create initial document for undo/redo functionality
        if (!m_ocafManager->NewDocument()) {
            QMessageBox::critical(this, "Error", "Failed to create new OCAF document");
            return false;
        }

        // Set initial view and render
        m_viewer->FitAll();
        m_viewer->RedrawAll();  // Ensure the coordinate trihedron is shown immediately


        return true;
    }

    void MainWindow::CreateActions() {
        // File actions
        m_newAction = new QAction("&New", this);
        m_newAction->setShortcut(QKeySequence::New);
        m_newAction->setStatusTip("Create a new document");

        m_openAction = new QAction("&Open...", this);
        m_openAction->setShortcut(QKeySequence::Open);
        m_openAction->setStatusTip("Open an existing document");

        m_saveAction = new QAction("&Save", this);
        m_saveAction->setShortcut(QKeySequence::Save);
        m_saveAction->setStatusTip("Save the document");

        m_saveAsAction = new QAction("Save &As...", this);
        m_saveAsAction->setShortcut(QKeySequence::SaveAs);
        m_saveAsAction->setStatusTip("Save the document with a new name");

        m_exitAction = new QAction("E&xit", this);
        m_exitAction->setShortcut(QKeySequence::Quit);
        m_exitAction->setStatusTip("Exit the application");

        // Edit actions
        m_undoAction = new QAction("&Undo", this);
        m_undoAction->setShortcut(QKeySequence::Undo);
        m_undoAction->setStatusTip("Undo the last operation");

        m_redoAction = new QAction("&Redo", this);
        m_redoAction->setShortcut(QKeySequence("Ctrl+Y"));
        m_redoAction->setStatusTip("Redo the last undone operation");

        // View actions
        m_fitAllAction = new QAction("Fit &All", this);
        m_fitAllAction->setShortcut(QKeySequence("F"));
        m_fitAllAction->setStatusTip("Fit all objects in view");

        m_zoomInAction = new QAction("Zoom &In", this);
        m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
        m_zoomInAction->setStatusTip("Zoom in");

        m_zoomOutAction = new QAction("Zoom &Out", this);
        m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
        m_zoomOutAction->setStatusTip("Zoom out");

        // View mode actions
        m_viewWireframeAction = new QAction("&Wireframe", this);
        m_viewWireframeAction->setShortcut(QKeySequence("W"));
        m_viewWireframeAction->setCheckable(true);
        m_viewWireframeAction->setStatusTip("Show wireframe view");

        m_viewShadedAction = new QAction("&Shaded", this);
        m_viewShadedAction->setShortcut(QKeySequence("S"));
        m_viewShadedAction->setCheckable(true);
        m_viewShadedAction->setChecked(true);
        m_viewShadedAction->setStatusTip("Show shaded view");

        m_viewModeGroup = new QActionGroup(this);
        m_viewModeGroup->addAction(m_viewWireframeAction);
        m_viewModeGroup->addAction(m_viewShadedAction);

        // Projection mode actions
        m_viewOrthographicAction = new QAction("&Orthographic", this);
        m_viewOrthographicAction->setCheckable(true);
        m_viewOrthographicAction->setChecked(true);
        m_viewOrthographicAction->setStatusTip("Orthographic projection");

        m_viewPerspectiveAction = new QAction("&Perspective", this);
        m_viewPerspectiveAction->setCheckable(true);
        m_viewPerspectiveAction->setStatusTip("Perspective projection");

        // Transparency action
        m_setTransparencyAction = new QAction("Set the transparency to 50%.", this);
        m_setTransparencyAction->setShortcut(QKeySequence("T"));
        m_setTransparencyAction->setStatusTip("Set all models to 50% transparency");

        // Shape transparency action
        m_setShapeTransparencyAction = new QAction("Set Selected Transparency", this);
        m_setShapeTransparencyAction->setStatusTip("Set transparency for the selected shape");


        m_projectionModeGroup = new QActionGroup(this);
        m_projectionModeGroup->addAction(m_viewOrthographicAction);
        m_projectionModeGroup->addAction(m_viewPerspectiveAction);

        // Create actions with 30x30 icons (icon-only display)
        m_createFaceAction = new QAction("", this);
        m_createFaceAction->setText("Face");
        m_createFaceAction->setStatusTip("Create a face");

        m_createBoxAction = new QAction("", this);
        m_createBoxAction->setText("Box");
        m_createBoxAction->setStatusTip("Create a box");

        m_createCylinderAction = new QAction("", this);
        m_createCylinderAction->setText("Cylinder");
        m_createCylinderAction->setStatusTip("Create a cylinder");

        m_createSphereAction = new QAction("", this);
        m_createSphereAction->setText("Sphere");
        m_createSphereAction->setStatusTip("Create a sphere");

        m_createExtrudeAction = new QAction("", this);
        m_createExtrudeAction->setText("Extrude");
        m_createExtrudeAction->setStatusTip("Create an extrude feature");

        m_createRevolveAction = new QAction("", this);
        m_createRevolveAction->setText("Revolve");
        m_createRevolveAction->setStatusTip("Create a revolve feature");

        m_createSweepAction = new QAction("", this);
        m_createSweepAction->setText("Sweep");
        m_createSweepAction->setStatusTip("Create a sweep feature");

        m_createLoftAction = new QAction("", this);
        m_createLoftAction->setText("Loft");
        m_createLoftAction->setStatusTip("Create a loft feature");

        // Boolean operations with 30x30 icons (icon-only display)
        m_booleanUnionAction = new QAction("", this);
        m_booleanUnionAction->setText("Union");
        m_booleanUnionAction->setStatusTip("Merge the selected shapes");

        m_booleanIntersectionAction = new QAction("", this);
        m_booleanIntersectionAction->setText("Intersection");
        m_booleanIntersectionAction->setStatusTip("Get the intersection of the selected shapes");

        m_booleanDifferenceAction = new QAction("", this);
        m_booleanDifferenceAction->setText("Difference");
        m_booleanDifferenceAction->setStatusTip("Subtract one shape from another shape");

        // Fillet and chamfer operations with 30x30 icons (icon-only display)
        m_filletAction = new QAction("", this);
        m_filletAction->setText("Fillet");
        m_filletAction->setStatusTip("Add fillet to selected edges");

        m_chamferAction = new QAction("", this);
        m_chamferAction->setText("Chamfer");
        m_chamferAction->setStatusTip("Add chamfer to selected edges");

        // Transform actions
        m_transformAction = new QAction("&Transform...", this);
        m_transformAction->setShortcut(QKeySequence("Ctrl+T"));
        m_transformAction->setStatusTip("Transform objects (translate, rotate, scale)");

        // Sketch actions
        m_enterSketchAction = new QAction("Enter &Sketch", this);
        m_enterSketchAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
        m_enterSketchAction->setStatusTip("Enter sketch mode");

        m_exitSketchAction = new QAction("E&xit Sketch", this);
        m_exitSketchAction->setShortcut(QKeySequence("Escape"));
        m_exitSketchAction->setStatusTip("Exit sketch mode");
        m_exitSketchAction->setEnabled(false);

        m_sketchRectangleAction = new QAction("&Rectangle", this);
        m_sketchRectangleAction->setCheckable(true);
        m_sketchRectangleAction->setShortcut(QKeySequence("R"));
        m_sketchRectangleAction->setStatusTip("Draw rectangle in sketch mode");
        m_sketchRectangleAction->setEnabled(false);

        m_sketchPointAction = new QAction("&Point", this);
        m_sketchPointAction->setCheckable(true);
        m_sketchPointAction->setShortcut(QKeySequence("P"));
        m_sketchPointAction->setStatusTip("Draw a point in sketch mode");
        m_sketchPointAction->setEnabled(false);

        m_sketchLineAction = new QAction("&Line", this);
        m_sketchLineAction->setCheckable(true);
        m_sketchLineAction->setShortcut(QKeySequence("L"));
        m_sketchLineAction->setStatusTip("Draw line in sketch mode");
        m_sketchLineAction->setEnabled(false);

        m_sketchCircleAction = new QAction("&Circle", this);
        m_sketchCircleAction->setCheckable(true);
        m_sketchCircleAction->setShortcut(QKeySequence("C"));
        m_sketchCircleAction->setStatusTip("Draw circle in sketch mode");
        m_sketchCircleAction->setEnabled(false);

        m_sketchArcAction = new QAction("&Arc", this);
        m_sketchArcAction->setCheckable(true);
        m_sketchArcAction->setShortcut(QKeySequence("A"));
        m_sketchArcAction->setStatusTip("Draw arc in sketch mode");
        m_sketchArcAction->setEnabled(false);

        m_sketchCurveAction = new QAction("C&urve", this);
        m_sketchCurveAction->setCheckable(true);
        m_sketchCurveAction->setStatusTip("Draw spline curve in sketch mode");
        m_sketchCurveAction->setEnabled(false);

        // ---- Constraint Actions ----
        m_constraintHorizontalAction = new QAction("Horizontal", this);
        m_constraintHorizontalAction->setStatusTip("Make line horizontal");
        m_constraintHorizontalAction->setEnabled(false);

        m_constraintVerticalAction = new QAction("Vertical", this);
        m_constraintVerticalAction->setStatusTip("Make line vertical");
        m_constraintVerticalAction->setEnabled(false);

        m_constraintCoincidentAction = new QAction("Coincident", this);
        m_constraintCoincidentAction->setStatusTip("Make two points coincident");
        m_constraintCoincidentAction->setEnabled(false);

        m_constraintDistanceAction = new QAction("Distance", this);
        m_constraintDistanceAction->setStatusTip("Set distance between two points");
        m_constraintDistanceAction->setEnabled(false);

        m_constraintParallelAction = new QAction("Parallel", this);
        m_constraintParallelAction->setStatusTip("Make two lines parallel");
        m_constraintParallelAction->setEnabled(false);

        m_constraintPerpendicularAction = new QAction("Perpendicular", this);
        m_constraintPerpendicularAction->setStatusTip("Make two lines perpendicular");
        m_constraintPerpendicularAction->setEnabled(false);

        m_constraintAngleAction = new QAction("Angle", this);
        m_constraintAngleAction->setStatusTip("Set angle between two lines");
        m_constraintAngleAction->setEnabled(false);

        m_constraintEqualLengthAction = new QAction("Equal", this);
        m_constraintEqualLengthAction->setStatusTip("Make two lines equal length");
        m_constraintEqualLengthAction->setEnabled(false);

        m_constraintFixedAction = new QAction("Fixed", this);
        m_constraintFixedAction->setStatusTip("Fix point at current position");
        m_constraintFixedAction->setEnabled(false);

        m_constraintRadiusAction = new QAction("Radius", this);
        m_constraintRadiusAction->setStatusTip("Set circle/arc radius");
        m_constraintRadiusAction->setEnabled(false);

        // Theme actions
        m_darkThemeAction = new QAction("&Dark Theme", this);
        m_darkThemeAction->setCheckable(true);
        m_darkThemeAction->setStatusTip("Use dark theme");

        m_lightThemeAction = new QAction("&Light Theme", this);
        m_lightThemeAction->setCheckable(true);
        m_lightThemeAction->setChecked(true);
        m_lightThemeAction->setStatusTip("Use light theme");

        m_themeGroup = new QActionGroup(this);
        m_themeGroup->addAction(m_darkThemeAction);
        m_themeGroup->addAction(m_lightThemeAction);

        // Help actions
        m_aboutAction = new QAction("&About", this);
        m_aboutAction->setStatusTip("Show the application's About box");

        m_aboutQtAction = new QAction("About &Qt", this);
        m_aboutQtAction->setStatusTip("Show the Qt library's About box");
    }

    void MainWindow::CreateMenus() {
        // File menu
        QMenu* fileMenu = menuBar()->addMenu("&File");
        fileMenu->addAction(m_newAction);
        fileMenu->addAction(m_openAction);
        fileMenu->addSeparator();
        fileMenu->addAction(m_saveAction);
        fileMenu->addAction(m_saveAsAction);
        fileMenu->addSeparator();
        fileMenu->addAction(m_exitAction);

        // Edit menu
        QMenu* editMenu = menuBar()->addMenu("&Edit");
        editMenu->addAction(m_undoAction);
        editMenu->addAction(m_redoAction);

        // View menu
        QMenu* viewMenu = menuBar()->addMenu("&View");
        viewMenu->addAction(m_fitAllAction);
        viewMenu->addAction(m_zoomInAction);
        viewMenu->addAction(m_zoomOutAction);
        viewMenu->addSeparator();
        viewMenu->addAction(m_viewWireframeAction);
        viewMenu->addAction(m_viewShadedAction);
        viewMenu->addSeparator();
        viewMenu->addAction(m_viewOrthographicAction);
        viewMenu->addAction(m_viewPerspectiveAction);
        viewMenu->addSeparator();
        viewMenu->addAction(m_setTransparencyAction);
        viewMenu->addAction(m_setShapeTransparencyAction);

        // Create menu
        QMenu* createMenu = menuBar()->addMenu("&Create");
        createMenu->addAction(m_createBoxAction);
        createMenu->addAction(m_createCylinderAction);
        createMenu->addAction(m_createSphereAction);
        createMenu->addSeparator();
        createMenu->addAction(m_createExtrudeAction);
        createMenu->addAction(m_createSweepAction);
        createMenu->addAction(m_createLoftAction);
        createMenu->addAction(m_createRevolveAction);

        // Boolean menu
        QMenu* booleanMenu = menuBar()->addMenu("&Boolean");
        booleanMenu->addAction(m_booleanUnionAction);
        booleanMenu->addAction(m_booleanIntersectionAction);
        booleanMenu->addAction(m_booleanDifferenceAction);

        // Modify menu
        QMenu* modifyMenu = menuBar()->addMenu("&Modify");
        modifyMenu->addAction(m_filletAction);
        modifyMenu->addAction(m_chamferAction);
        modifyMenu->addSeparator();
        modifyMenu->addAction(m_transformAction);

        // Sketch menu
        QMenu* sketchMenu = menuBar()->addMenu("&Sketch");
        sketchMenu->addAction(m_enterSketchAction);
        sketchMenu->addAction(m_exitSketchAction);
        sketchMenu->addSeparator();
        sketchMenu->addAction(m_sketchRectangleAction);
        sketchMenu->addAction(m_sketchPointAction);
        sketchMenu->addAction(m_sketchLineAction);
        sketchMenu->addAction(m_sketchCircleAction);
        sketchMenu->addAction(m_sketchArcAction);
        sketchMenu->addAction(m_sketchCurveAction);

        sketchMenu->addSeparator();
        QMenu* constraintMenu = sketchMenu->addMenu("Constraints");
        constraintMenu->addAction(m_constraintHorizontalAction);
        constraintMenu->addAction(m_constraintVerticalAction);
        constraintMenu->addAction(m_constraintCoincidentAction);
        constraintMenu->addAction(m_constraintDistanceAction);
        constraintMenu->addAction(m_constraintParallelAction);
        constraintMenu->addAction(m_constraintPerpendicularAction);
        constraintMenu->addAction(m_constraintAngleAction);
        constraintMenu->addAction(m_constraintEqualLengthAction);
        constraintMenu->addAction(m_constraintFixedAction);
        constraintMenu->addAction(m_constraintRadiusAction);

        // Tools menu
        QMenu* toolsMenu = menuBar()->addMenu("&Tools");
        toolsMenu->addAction(m_darkThemeAction);
        toolsMenu->addAction(m_lightThemeAction);

        // Help menu
        QMenu* helpMenu = menuBar()->addMenu("&Help");
        helpMenu->addAction(m_aboutAction);
        helpMenu->addAction(m_aboutQtAction);
    }

    void MainWindow::CreateToolBars() {
        // Create main toolbar area widget with tabs
        QWidget* toolBarArea = new QWidget(this);
        toolBarArea->setObjectName("toolBarArea");
        toolBarArea->setMaximumHeight(180);
        toolBarArea->setMinimumHeight(180);

        // Create tab widget for organizing tools
        QTabWidget* toolTabWidget = new QTabWidget(toolBarArea);
        toolTabWidget->setObjectName("toolTabWidget");
        toolTabWidget->setTabPosition(QTabWidget::North);

        // File Tab - File operations and undo/redo
        QWidget* fileTab = new QWidget();
        QHBoxLayout* fileLayout = new QHBoxLayout(fileTab);
        fileLayout->setContentsMargins(5, 5, 5, 5);
        fileLayout->setSpacing(3);

        // File operations group
        QFrame* fileOpsFrame = new QFrame();
        fileOpsFrame->setFrameStyle(QFrame::StyledPanel);
        QVBoxLayout* fileOpsLayout = new QVBoxLayout(fileOpsFrame);
        fileOpsLayout->setContentsMargins(2, 1, 2, 2);
        fileOpsLayout->setSpacing(1);

        QLabel* fileLabel = new QLabel("File");
        fileLabel->setAlignment(Qt::AlignCenter);
        fileOpsLayout->addWidget(fileLabel);
        QHBoxLayout* fileButtonsLayout = new QHBoxLayout();
        fileButtonsLayout->setSpacing(2);

        QToolButton* newBtn = new QToolButton();
        newBtn->setDefaultAction(m_newAction);
        newBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        newBtn->setMinimumSize(40, 40);
        newBtn->setMaximumSize(40, 40);
        fileButtonsLayout->addWidget(newBtn);

        QToolButton* openBtn = new QToolButton();
        openBtn->setDefaultAction(m_openAction);
        openBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        openBtn->setMinimumSize(90, 90);
        openBtn->setMaximumSize(90, 90);
        fileButtonsLayout->addWidget(openBtn);

        QToolButton* saveBtn = new QToolButton();
        saveBtn->setDefaultAction(m_saveAction);
        saveBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        saveBtn->setMinimumSize(90, 90);
        saveBtn->setMaximumSize(90, 90);
        fileButtonsLayout->addWidget(saveBtn);

        fileOpsLayout->addLayout(fileButtonsLayout);
        fileLayout->addWidget(fileOpsFrame);

        // History operations group
        QFrame* historyFrame = new QFrame();
        historyFrame->setFrameStyle(QFrame::StyledPanel);
        QVBoxLayout* historyLayout = new QVBoxLayout(historyFrame);
        historyLayout->setContentsMargins(2, 1, 2, 2);
        historyLayout->setSpacing(1);

        QLabel* historyLabel = new QLabel("History");
        historyLabel->setAlignment(Qt::AlignCenter);
        historyLayout->addWidget(historyLabel);
        QHBoxLayout* historyButtonsLayout = new QHBoxLayout();
        historyButtonsLayout->setSpacing(2);

        QToolButton* undoBtn = new QToolButton();
        undoBtn->setDefaultAction(m_undoAction);
        undoBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        undoBtn->setMinimumSize(90, 90);
        undoBtn->setMaximumSize(90, 90);
        historyButtonsLayout->addWidget(undoBtn);

        QToolButton* redoBtn = new QToolButton();
        redoBtn->setDefaultAction(m_redoAction);
        redoBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        redoBtn->setMinimumSize(90, 90);
        redoBtn->setMaximumSize(90, 90);
        historyButtonsLayout->addWidget(redoBtn);

        historyLayout->addLayout(historyButtonsLayout);
        fileLayout->addWidget(historyFrame);

        fileLayout->addStretch();
        toolTabWidget->addTab(fileTab, "File");

        // Design Tab - Primitive creation
        QWidget* designTab = new QWidget();
        QHBoxLayout* designLayout = new QHBoxLayout(designTab);
        designLayout->setContentsMargins(5, 5, 5, 5);
        designLayout->setSpacing(3);

        // Primitives group
        QFrame* primitivesFrame = new QFrame();
        primitivesFrame->setFrameStyle(QFrame::StyledPanel);
        QVBoxLayout* primitivesLayout = new QVBoxLayout(primitivesFrame);
        primitivesLayout->setContentsMargins(5, 3, 5, 8);
        primitivesLayout->setSpacing(1);

        QLabel* primitivesLabel = new QLabel("Basic Shapes");
        primitivesLabel->setAlignment(Qt::AlignCenter);
        primitivesLayout->addWidget(primitivesLabel);
        QHBoxLayout* primitivesButtonsLayout = new QHBoxLayout();
        primitivesButtonsLayout->setSpacing(8);

        // Face button with label below
        QVBoxLayout* faceLayout = new QVBoxLayout();
        QToolButton* faceBtn = new QToolButton();
        faceBtn->setDefaultAction(m_createFaceAction);
        faceBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        faceBtn->setIconSize(QSize(30, 30));
        faceBtn->setFixedSize(30, 30);
        QLabel* faceLabel = new QLabel("Face");
        faceLabel->setAlignment(Qt::AlignCenter);
        faceLabel->setStyleSheet("font-size: 9px; color: #333; margin-top: 2px;");
        faceLayout->addWidget(faceBtn);
        faceLayout->addWidget(faceLabel);
        faceLayout->setSpacing(1);
        faceLayout->setContentsMargins(0, 0, 0, 0);
        primitivesButtonsLayout->addLayout(faceLayout);

        // Box button with label below
        QVBoxLayout* boxLayout = new QVBoxLayout();
        QToolButton* boxBtn = new QToolButton();
        boxBtn->setDefaultAction(m_createBoxAction);
        boxBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        boxBtn->setIconSize(QSize(30, 30));
        boxBtn->setFixedSize(30, 30);
        QLabel* boxLabel = new QLabel("Box");
        boxLabel->setAlignment(Qt::AlignCenter);
        boxLabel->setStyleSheet("font-size: 9px; color: #333; margin-top: 2px;");
        boxLayout->addWidget(boxBtn);
        boxLayout->addWidget(boxLabel);
        boxLayout->setSpacing(1);
        boxLayout->setContentsMargins(0, 0, 0, 0);
        primitivesButtonsLayout->addLayout(boxLayout);

        // Cylinder button with label below
        QVBoxLayout* cylinderLayout = new QVBoxLayout();
        QToolButton* cylinderBtn = new QToolButton();
        cylinderBtn->setDefaultAction(m_createCylinderAction);
        cylinderBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        cylinderBtn->setIconSize(QSize(30, 30));
        cylinderBtn->setFixedSize(30, 30);
        QLabel* cylinderLabel = new QLabel("Cylinder");
        cylinderLabel->setAlignment(Qt::AlignCenter);
        cylinderLabel->setStyleSheet("font-size: 9px; color: #333; margin-top: 2px;");
        cylinderLayout->addWidget(cylinderBtn);
        cylinderLayout->addWidget(cylinderLabel);
        cylinderLayout->setSpacing(1);
        cylinderLayout->setContentsMargins(0, 0, 0, 0);
        primitivesButtonsLayout->addLayout(cylinderLayout);

        // Sphere button with label below
        QVBoxLayout* sphereLayout = new QVBoxLayout();
        QToolButton* sphereBtn = new QToolButton();
        sphereBtn->setDefaultAction(m_createSphereAction);
        sphereBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        sphereBtn->setIconSize(QSize(30, 30));
        sphereBtn->setFixedSize(30, 30);
        QLabel* sphereLabel = new QLabel("Sphere");
        sphereLabel->setAlignment(Qt::AlignCenter);
        sphereLabel->setStyleSheet("font-size: 9px; color: #333; margin-top: 2px;");
        sphereLayout->addWidget(sphereBtn);
        sphereLayout->addWidget(sphereLabel);
        sphereLayout->setSpacing(1);
        sphereLayout->setContentsMargins(0, 0, 0, 0);
        primitivesButtonsLayout->addLayout(sphereLayout);

        primitivesLayout->addLayout(primitivesButtonsLayout);
        designLayout->addWidget(primitivesFrame);

        // Features group
        QFrame* featuresFrame = new QFrame();
        featuresFrame->setFrameStyle(QFrame::StyledPanel);
        QVBoxLayout* featuresLayout = new QVBoxLayout(featuresFrame);
        featuresLayout->setContentsMargins(5, 3, 5, 8);
        featuresLayout->setSpacing(1);

        QLabel* featuresLabel = new QLabel("Features");
        featuresLabel->setAlignment(Qt::AlignCenter);
        featuresLayout->addWidget(featuresLabel);
        QHBoxLayout* featuresButtonsLayout = new QHBoxLayout();
        featuresButtonsLayout->setSpacing(8);

        // Extrude button with label below
        QVBoxLayout* extrudeLayout = new QVBoxLayout();
        QToolButton* extrudeBtn = new QToolButton();
        extrudeBtn->setDefaultAction(m_createExtrudeAction);
        extrudeBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        extrudeBtn->setIconSize(QSize(30, 30));
        extrudeBtn->setFixedSize(30, 30);
        QLabel* extrudeLabel = new QLabel("extrude");
        extrudeLabel->setAlignment(Qt::AlignCenter);
        extrudeLabel->setStyleSheet("font-size: 9px; color: #333; margin-top: 2px;");
        extrudeLayout->addWidget(extrudeBtn);
        extrudeLayout->addWidget(extrudeLabel);
        extrudeLayout->setSpacing(1);
        extrudeLayout->setContentsMargins(0, 0, 0, 0);
        featuresButtonsLayout->addLayout(extrudeLayout);

        QVBoxLayout* sweepLayout = new QVBoxLayout();
        QToolButton* sweepBtn = new QToolButton();
        sweepBtn->setDefaultAction(m_createSweepAction);
        sweepBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        sweepBtn->setIconSize(QSize(30, 30));
        sweepBtn->setFixedSize(30, 30);
        QLabel* sweepLabel = new QLabel("Sweep");
        sweepLabel->setAlignment(Qt::AlignCenter);
        sweepLabel->setStyleSheet("font-size: 9px; color: #333; margin-top: 2px;");
        sweepLayout->addWidget(sweepBtn);
        sweepLayout->addWidget(sweepLabel);
        sweepLayout->setSpacing(1);
        sweepLayout->setContentsMargins(0, 0, 0, 0);
        featuresButtonsLayout->addLayout(sweepLayout);

        QVBoxLayout* revolveLayout = new QVBoxLayout();
        QToolButton* revolveBtn = new QToolButton();
        revolveBtn->setDefaultAction(m_createRevolveAction);
        revolveBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        revolveBtn->setIconSize(QSize(30, 30));
        revolveBtn->setFixedSize(30, 30);
        QLabel* revolveLabel = new QLabel("revolve");
        revolveLabel->setAlignment(Qt::AlignCenter);
        revolveLabel->setStyleSheet("font-size: 9px; color: #333; margin-top: 2px;");
        revolveLayout->addWidget(revolveBtn);
        revolveLayout->addWidget(revolveLabel);
        revolveLayout->setSpacing(1);
        revolveLayout->setContentsMargins(0, 0, 0, 0);
        featuresButtonsLayout->addLayout(revolveLayout);

        QVBoxLayout* loftLayout = new QVBoxLayout();
        QToolButton* loftBtn = new QToolButton();
        loftBtn->setDefaultAction(m_createLoftAction);
        loftBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        loftBtn->setIconSize(QSize(30, 30));
        loftBtn->setFixedSize(30, 30);
        QLabel* loftLabel = new QLabel("Loft");
        loftLabel->setAlignment(Qt::AlignCenter);
        loftLabel->setStyleSheet("font-size: 9px; color: #333; margin-top: 2px;");
        loftLayout->addWidget(loftBtn);
        loftLayout->addWidget(loftLabel);
        loftLayout->setSpacing(1);
        loftLayout->setContentsMargins(0, 0, 0, 0);

        featuresButtonsLayout->addLayout(loftLayout);
        featuresLayout->addLayout(featuresButtonsLayout);
        designLayout->addWidget(featuresFrame);
        designLayout->addStretch();
        toolTabWidget->addTab(designTab, "Design");

        // Modify Tab - Boolean operations and modifications
        QWidget* modifyTab = new QWidget();
        QHBoxLayout* modifyLayout = new QHBoxLayout(modifyTab);
        modifyLayout->setContentsMargins(5, 5, 5, 5);
        modifyLayout->setSpacing(3);

        // Boolean operations group
        QFrame* booleanFrame = new QFrame();
        booleanFrame->setFrameStyle(QFrame::StyledPanel);
        QVBoxLayout* booleanLayout = new QVBoxLayout(booleanFrame);
        booleanLayout->setContentsMargins(5, 3, 5, 8);
        booleanLayout->setSpacing(1);

        QLabel* booleanLabel = new QLabel("Boolean");
        booleanLabel->setAlignment(Qt::AlignCenter);
        booleanLayout->addWidget(booleanLabel);
        QHBoxLayout* booleanButtonsLayout = new QHBoxLayout();
        booleanButtonsLayout->setSpacing(8);

        // Union button with label below
        QVBoxLayout* unionLayout = new QVBoxLayout();
        QToolButton* unionBtn = new QToolButton();
        unionBtn->setDefaultAction(m_booleanUnionAction);
        unionBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        unionBtn->setIconSize(QSize(30, 30));
        unionBtn->setFixedSize(30, 30);
        QLabel* unionLabel = new QLabel("Union");
        unionLabel->setAlignment(Qt::AlignCenter);
        unionLabel->setStyleSheet("font-size: 9px; color: #333; margin-top: 2px;");
        unionLayout->addWidget(unionBtn);
        unionLayout->addWidget(unionLabel);
        unionLayout->setSpacing(1);
        unionLayout->setContentsMargins(0, 0, 0, 0);
        booleanButtonsLayout->addLayout(unionLayout);

        // Intersection button with label below
        QVBoxLayout* intersectionLayout = new QVBoxLayout();
        QToolButton* intersectionBtn = new QToolButton();
        intersectionBtn->setDefaultAction(m_booleanIntersectionAction);
        intersectionBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        intersectionBtn->setIconSize(QSize(30, 30));
        intersectionBtn->setFixedSize(30, 30);
        QLabel* intersectionLabel = new QLabel("Intersection");
        intersectionLabel->setAlignment(Qt::AlignCenter);
        intersectionLabel->setStyleSheet("font-size: 9px; color: #333; margin-top: 2px;");
        intersectionLayout->addWidget(intersectionBtn);
        intersectionLayout->addWidget(intersectionLabel);
        intersectionLayout->setSpacing(1);
        intersectionLayout->setContentsMargins(0, 0, 0, 0);
        booleanButtonsLayout->addLayout(intersectionLayout);

        // Difference button with label below
        QVBoxLayout* differenceLayout = new QVBoxLayout();
        QToolButton* differenceBtn = new QToolButton();
        differenceBtn->setDefaultAction(m_booleanDifferenceAction);
        differenceBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        differenceBtn->setIconSize(QSize(30, 30));
        differenceBtn->setFixedSize(30, 30);
        QLabel* differenceLabel = new QLabel("Difference");
        differenceLabel->setAlignment(Qt::AlignCenter);
        differenceLabel->setStyleSheet("font-size: 9px; color: #333; margin-top: 2px;");
        differenceLayout->addWidget(differenceBtn);
        differenceLayout->addWidget(differenceLabel);
        differenceLayout->setSpacing(1);
        differenceLayout->setContentsMargins(0, 0, 0, 0);
        booleanButtonsLayout->addLayout(differenceLayout);

        booleanLayout->addLayout(booleanButtonsLayout);
        modifyLayout->addWidget(booleanFrame);

        // Modifications group
        QFrame* modificationsFrame = new QFrame();
        modificationsFrame->setFrameStyle(QFrame::StyledPanel);
        QVBoxLayout* modificationsLayout = new QVBoxLayout(modificationsFrame);
        modificationsLayout->setContentsMargins(5, 3, 5, 8);
        modificationsLayout->setSpacing(1);

        QLabel* modificationsLabel = new QLabel("modification");
        modificationsLabel->setAlignment(Qt::AlignCenter);
        modificationsLayout->addWidget(modificationsLabel);
        QHBoxLayout* modificationsButtonsLayout = new QHBoxLayout();
        modificationsButtonsLayout->setSpacing(8);

        // Fillet button with label below
        QVBoxLayout* filletLayout = new QVBoxLayout();
        QToolButton* filletBtn = new QToolButton();
        filletBtn->setDefaultAction(m_filletAction);
        filletBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        filletBtn->setIconSize(QSize(30, 30));
        filletBtn->setFixedSize(30, 30);
        QLabel* filletLabel = new QLabel("fillet");
        filletLabel->setAlignment(Qt::AlignCenter);
        filletLabel->setStyleSheet("font-size: 9px; color: #333; margin-top: 2px;");
        filletLayout->addWidget(filletBtn);
        filletLayout->addWidget(filletLabel);
        filletLayout->setSpacing(1);
        filletLayout->setContentsMargins(0, 0, 0, 0);
        modificationsButtonsLayout->addLayout(filletLayout);

        // Chamfer button with label below
        QVBoxLayout* chamferLayout = new QVBoxLayout();
        QToolButton* chamferBtn = new QToolButton();
        chamferBtn->setDefaultAction(m_chamferAction);
        chamferBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        chamferBtn->setIconSize(QSize(30, 30));
        chamferBtn->setFixedSize(30, 30);
        QLabel* chamferLabel = new QLabel("Chamfer");
        chamferLabel->setAlignment(Qt::AlignCenter);
        chamferLabel->setStyleSheet("font-size: 9px; color: #333; margin-top: 2px;");
        chamferLayout->addWidget(chamferBtn);
        chamferLayout->addWidget(chamferLabel);
        chamferLayout->setSpacing(1);
        chamferLayout->setContentsMargins(0, 0, 0, 0);
        modificationsButtonsLayout->addLayout(chamferLayout);

        // Transform button with label below
        QVBoxLayout* transformLayout = new QVBoxLayout();
        QToolButton* transformBtn = new QToolButton();
        transformBtn->setDefaultAction(m_transformAction);
        transformBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        transformBtn->setFixedSize(30, 30);
        QLabel* transformLabel = new QLabel("Transform");
        transformLabel->setAlignment(Qt::AlignCenter);
        transformLabel->setStyleSheet("font-size: 9px; color: #333; margin-top: 2px;");
        transformLayout->addWidget(transformBtn);
        transformLayout->addWidget(transformLabel);
        transformLayout->setSpacing(1);
        transformLayout->setContentsMargins(0, 0, 0, 0);
        modificationsButtonsLayout->addLayout(transformLayout);

        modificationsLayout->addLayout(modificationsButtonsLayout);
        modifyLayout->addWidget(modificationsFrame);

        modifyLayout->addStretch();
        toolTabWidget->addTab(modifyTab, "Modify");

        // View Tab - View controls and selection
        QWidget* viewTab = new QWidget();
        QHBoxLayout* viewLayout = new QHBoxLayout(viewTab);
        viewLayout->setContentsMargins(5, 2, 5, 2);
        viewLayout->setSpacing(3);

        // Selection group
        QFrame* selectionFrame = new QFrame();
        selectionFrame->setFrameStyle(QFrame::StyledPanel);
        QVBoxLayout* selectionLayout = new QVBoxLayout(selectionFrame);
        selectionLayout->setContentsMargins(2, 1, 2, 2);
        selectionLayout->setSpacing(1);

        QLabel* selectionLabel = new QLabel("Select");
        selectionLabel->setAlignment(Qt::AlignCenter);
        selectionLayout->addWidget(selectionLabel);

        // Add selection mode combo box
        if (m_selectionModeCombo) {
            m_selectionModeCombo->setMinimumWidth(100);
            selectionLayout->addWidget(m_selectionModeCombo);
        }

        viewLayout->addWidget(selectionFrame);

        // View controls group
        QFrame* viewControlsFrame = new QFrame();
        viewControlsFrame->setFrameStyle(QFrame::StyledPanel);
        QVBoxLayout* viewControlsLayout = new QVBoxLayout(viewControlsFrame);
        viewControlsLayout->setContentsMargins(2, 1, 2, 2);
        viewControlsLayout->setSpacing(1);

        QLabel* viewControlsLabel = new QLabel("View");
        viewControlsLabel->setAlignment(Qt::AlignCenter);
        viewControlsLayout->addWidget(viewControlsLabel);
        QHBoxLayout* viewControlsButtonsLayout = new QHBoxLayout();
        viewControlsButtonsLayout->setSpacing(2);

        QToolButton* fitAllBtn = new QToolButton();
        fitAllBtn->setDefaultAction(m_fitAllAction);
        fitAllBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        viewControlsButtonsLayout->addWidget(fitAllBtn);

        QToolButton* wireframeBtn = new QToolButton();
        wireframeBtn->setDefaultAction(m_viewWireframeAction);
        wireframeBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        viewControlsButtonsLayout->addWidget(wireframeBtn);

        QToolButton* shadedBtn = new QToolButton();
        shadedBtn->setDefaultAction(m_viewShadedAction);
        shadedBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        viewControlsButtonsLayout->addWidget(shadedBtn);

        QToolButton* transparencyBtn = new QToolButton();
        transparencyBtn->setDefaultAction(m_setTransparencyAction);
        transparencyBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        viewControlsButtonsLayout->addWidget(transparencyBtn);

        QToolButton* shapeTransparencyBtn = new QToolButton();
        shapeTransparencyBtn->setDefaultAction(m_setShapeTransparencyAction);
        shapeTransparencyBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        viewControlsButtonsLayout->addWidget(shapeTransparencyBtn);


        viewControlsLayout->addLayout(viewControlsButtonsLayout);
        viewLayout->addWidget(viewControlsFrame);

        viewLayout->addStretch();
        toolTabWidget->addTab(viewTab, "View");

        // Sketch Tab - Sketch mode controls
        QWidget* sketchTab = new QWidget();
        QHBoxLayout* sketchLayout = new QHBoxLayout(sketchTab);
        sketchLayout->setContentsMargins(5, 2, 5, 2);
        sketchLayout->setSpacing(3);

        // Sketch mode group
        QFrame* sketchModeFrame = new QFrame();
        sketchModeFrame->setFrameStyle(QFrame::StyledPanel);
        QVBoxLayout* sketchModeLayout = new QVBoxLayout(sketchModeFrame);
        sketchModeLayout->setContentsMargins(2, 1, 2, 2);
        sketchModeLayout->setSpacing(1);

        QLabel* sketchModeLabel = new QLabel("SketchModeL");
        sketchModeLabel->setAlignment(Qt::AlignCenter);
        sketchModeLayout->addWidget(sketchModeLabel);
        QHBoxLayout* sketchModeButtonsLayout = new QHBoxLayout();
        sketchModeButtonsLayout->setSpacing(2);

        QToolButton* enterSketchBtn = new QToolButton();
        enterSketchBtn->setDefaultAction(m_enterSketchAction);
        enterSketchBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        sketchModeButtonsLayout->addWidget(enterSketchBtn);

        QToolButton* exitSketchBtn = new QToolButton();
        exitSketchBtn->setDefaultAction(m_exitSketchAction);
        exitSketchBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        sketchModeButtonsLayout->addWidget(exitSketchBtn);

        sketchModeLayout->addLayout(sketchModeButtonsLayout);
        sketchLayout->addWidget(sketchModeFrame);

        // Sketch tools group
        QFrame* sketchToolsFrame = new QFrame();
        sketchToolsFrame->setFrameStyle(QFrame::StyledPanel);
        QVBoxLayout* sketchToolsLayout = new QVBoxLayout(sketchToolsFrame);
        sketchToolsLayout->setContentsMargins(2, 1, 2, 2);
        sketchToolsLayout->setSpacing(1);

        QLabel* sketchToolsLabel = new QLabel("SketchTool");
        sketchToolsLabel->setAlignment(Qt::AlignCenter);
        sketchToolsLayout->addWidget(sketchToolsLabel);
        QHBoxLayout* sketchToolsButtonsLayout = new QHBoxLayout();
        sketchToolsButtonsLayout->setSpacing(2);

        QToolButton* rectangleBtn = new QToolButton();
        rectangleBtn->setDefaultAction(m_sketchRectangleAction);
        rectangleBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        sketchToolsButtonsLayout->addWidget(rectangleBtn);

        QToolButton* pointBtn = new QToolButton();
        pointBtn->setDefaultAction(m_sketchPointAction);
        pointBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        sketchToolsButtonsLayout->addWidget(pointBtn);

        QToolButton* lineBtn = new QToolButton();
        lineBtn->setDefaultAction(m_sketchLineAction);
        lineBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        sketchToolsButtonsLayout->addWidget(lineBtn);

        QToolButton* circleBtn = new QToolButton();
        circleBtn->setDefaultAction(m_sketchCircleAction);
        circleBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        sketchToolsButtonsLayout->addWidget(circleBtn);

        QToolButton* arcBtn = new QToolButton();
        arcBtn->setDefaultAction(m_sketchArcAction);
        arcBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        sketchToolsButtonsLayout->addWidget(arcBtn);

        QToolButton* curveBtn = new QToolButton();
        curveBtn->setDefaultAction(m_sketchCurveAction);
        curveBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        sketchToolsButtonsLayout->addWidget(curveBtn);

        sketchToolsLayout->addLayout(sketchToolsButtonsLayout);
        sketchLayout->addWidget(sketchToolsFrame);

        // Constraint tools group
        QFrame* constraintFrame = new QFrame();
        constraintFrame->setFrameStyle(QFrame::StyledPanel);
        QVBoxLayout* constraintMainLayout = new QVBoxLayout(constraintFrame);
        constraintMainLayout->setContentsMargins(2, 1, 2, 2);
        constraintMainLayout->setSpacing(1);

        QLabel* constraintLabel = new QLabel("Constraints");
        constraintLabel->setAlignment(Qt::AlignCenter);
        constraintMainLayout->addWidget(constraintLabel);

        // Two-row button layout

        QHBoxLayout* constraintRow1 = new QHBoxLayout();
        constraintRow1->setSpacing(2);
        QHBoxLayout* constraintRow2 = new QHBoxLayout();
        constraintRow2->setSpacing(2);

        auto makeConstraintBtn = [](QAction* action) {
            QToolButton* btn = new QToolButton();
            btn->setDefaultAction(action);
            btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
            btn->setFixedHeight(25);
            return btn;
            };

        // First row: Horizontal, Vertical, Coincident, Distance, Fixed

        constraintRow1->addWidget(makeConstraintBtn(m_constraintHorizontalAction));
        constraintRow1->addWidget(makeConstraintBtn(m_constraintVerticalAction));
        constraintRow1->addWidget(makeConstraintBtn(m_constraintCoincidentAction));
        constraintRow1->addWidget(makeConstraintBtn(m_constraintDistanceAction));
        constraintRow1->addWidget(makeConstraintBtn(m_constraintFixedAction));

        // Second row: Parallel, Perpendicular, Angle, Equal, Radius

        constraintRow2->addWidget(makeConstraintBtn(m_constraintParallelAction));
        constraintRow2->addWidget(makeConstraintBtn(m_constraintPerpendicularAction));
        constraintRow2->addWidget(makeConstraintBtn(m_constraintAngleAction));
        constraintRow2->addWidget(makeConstraintBtn(m_constraintEqualLengthAction));
        constraintRow2->addWidget(makeConstraintBtn(m_constraintRadiusAction));

        constraintMainLayout->addLayout(constraintRow1);
        constraintMainLayout->addLayout(constraintRow2);
        sketchLayout->addWidget(constraintFrame);

        sketchLayout->addStretch();
        toolTabWidget->addTab(sketchTab, "Sketch");

        // Set layout for toolbar area
        QVBoxLayout* toolBarAreaLayout = new QVBoxLayout(toolBarArea);
        toolBarAreaLayout->setContentsMargins(0, 0, 0, 0);
        toolBarAreaLayout->addWidget(toolTabWidget);

        // Add toolbar area as a toolbar to maintain proper positioning
        QToolBar* containerToolBar = addToolBar("Container");
        containerToolBar->addWidget(toolBarArea);
        containerToolBar->setMovable(false);
        containerToolBar->setObjectName("containerToolBar");

        // Set selection mode buttons object names for styling
        // Old selection mode button styling removed - now using combo box

        // Set boolean operation buttons object names
        m_booleanUnionAction->setObjectName("booleanButton");
        m_booleanIntersectionAction->setObjectName("booleanButton");
        m_booleanDifferenceAction->setObjectName("booleanButton");

        // Set modify operation buttons object names
        m_filletAction->setObjectName("modifyButton");
        m_chamferAction->setObjectName("modifyButton");
    }

    void MainWindow::CreateStatusBar() {
        statusBar()->showMessage("Ready");
    }

    void MainWindow::CreateDockWidgets() {
        // Document tree dock
        m_documentDock = new QDockWidget("Document Tree", this);
        m_documentTree = new DocumentTree(this);
        m_documentDock->setWidget(m_documentTree);
        addDockWidget(Qt::LeftDockWidgetArea, m_documentDock);

        // Property panel dock
        m_propertyDock = new QDockWidget("Properties", this);
        m_propertyPanel = new PropertyPanel(this);
        m_propertyDock->setWidget(m_propertyPanel);
        addDockWidget(Qt::RightDockWidgetArea, m_propertyDock);
    }

    void MainWindow::ConnectSignals() {
        // File actions
        connect(m_newAction, &QAction::triggered, this, &MainWindow::OnNewDocument);
        connect(m_openAction, &QAction::triggered, this, &MainWindow::OnOpenDocument);
        connect(m_saveAction, &QAction::triggered, this, &MainWindow::OnSaveDocument);
        connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::OnSaveDocumentAs);
        connect(m_exitAction, &QAction::triggered, this, &MainWindow::OnExit);

        // Edit actions
        connect(m_undoAction, &QAction::triggered, this, &MainWindow::OnUndo);
        connect(m_redoAction, &QAction::triggered, this, &MainWindow::OnRedo);

        // View actions
        connect(m_fitAllAction, &QAction::triggered, this, &MainWindow::OnFitAll);
        connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::OnZoomIn);
        connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::OnZoomOut);
        connect(m_viewWireframeAction, &QAction::triggered, this, &MainWindow::OnViewWireframe);
        connect(m_viewShadedAction, &QAction::triggered, this, &MainWindow::OnViewShaded);
        connect(m_viewOrthographicAction, &QAction::triggered, this, &MainWindow::OnViewOrthographic);
        connect(m_viewPerspectiveAction, &QAction::triggered, this, &MainWindow::OnViewPerspective);
        connect(m_setTransparencyAction, &QAction::triggered, this, &MainWindow::OnSetTransparency);
        connect(m_setShapeTransparencyAction, &QAction::triggered, this, &MainWindow::OnSetShapeTransparency);

        // Create actions
        connect(m_createFaceAction, &QAction::triggered, this, &MainWindow::OnCreateFace);
        connect(m_createBoxAction, &QAction::triggered, this, &MainWindow::OnCreateBox);
        connect(m_createCylinderAction, &QAction::triggered, this, &MainWindow::OnCreateCylinder);
        connect(m_createSphereAction, &QAction::triggered, this, &MainWindow::OnCreateSphere);
        connect(m_createExtrudeAction, &QAction::triggered, this, &MainWindow::OnCreateExtrude);
        connect(m_createRevolveAction, &QAction::triggered, this, &MainWindow::OnCreateRevolve);
        connect(m_createSweepAction, &QAction::triggered, this, &MainWindow::OnCreateSweep);
        connect(m_createLoftAction, &QAction::triggered, this, &MainWindow::OnCreateLoft);

        // Boolean actions
        connect(m_booleanUnionAction, &QAction::triggered, this, &MainWindow::OnBooleanUnion);
        connect(m_booleanIntersectionAction, &QAction::triggered, this, &MainWindow::OnBooleanIntersection);
        connect(m_booleanDifferenceAction, &QAction::triggered, this, &MainWindow::OnBooleanDifference);

        // Modify actions
        connect(m_filletAction, &QAction::triggered, this, &MainWindow::OnFillet);
        connect(m_chamferAction, &QAction::triggered, this, &MainWindow::OnChamfer);

        // Transform operations
        connect(m_transformAction, &QAction::triggered, this, &MainWindow::OnTransformObjects);

        // Sketch actions
        connect(m_enterSketchAction, &QAction::triggered, this, &MainWindow::OnEnterSketchMode);
        connect(m_exitSketchAction, &QAction::triggered, this, &MainWindow::OnExitSketchMode);
        connect(m_sketchRectangleAction, &QAction::triggered, this, &MainWindow::OnSketchRectangleTool);
        connect(m_sketchPointAction, &QAction::triggered, this, &MainWindow::OnSketchPointTool);
        connect(m_sketchLineAction, &QAction::triggered, this, &MainWindow::OnSketchLineTool);
        connect(m_sketchCircleAction, &QAction::triggered, this, &MainWindow::OnSketchCircleTool);
        connect(m_sketchArcAction, &QAction::triggered, this, &MainWindow::OnSketchArcTool);
        connect(m_sketchCurveAction, &QAction::triggered, this, &MainWindow::OnSketchCurveTool);

        // Constraint actions
        connect(m_constraintHorizontalAction, &QAction::triggered, this, &MainWindow::OnConstraintHorizontal);
        connect(m_constraintVerticalAction, &QAction::triggered, this, &MainWindow::OnConstraintVertical);
        connect(m_constraintCoincidentAction, &QAction::triggered, this, &MainWindow::OnConstraintCoincident);
        connect(m_constraintDistanceAction, &QAction::triggered, this, &MainWindow::OnConstraintDistance);
        connect(m_constraintParallelAction, &QAction::triggered, this, &MainWindow::OnConstraintParallel);
        connect(m_constraintPerpendicularAction, &QAction::triggered, this, &MainWindow::OnConstraintPerpendicular);
        connect(m_constraintAngleAction, &QAction::triggered, this, &MainWindow::OnConstraintAngle);
        connect(m_constraintEqualLengthAction, &QAction::triggered, this, &MainWindow::OnConstraintEqualLength);
        connect(m_constraintFixedAction, &QAction::triggered, this, &MainWindow::OnConstraintFixed);
        connect(m_constraintRadiusAction, &QAction::triggered, this, &MainWindow::OnConstraintRadius);

        // Selection mode combo box connected in CreateSelectionModeCombo()

        // Theme actions
        connect(m_darkThemeAction, &QAction::triggered, this, &MainWindow::OnDarkTheme);
        connect(m_lightThemeAction, &QAction::triggered, this, &MainWindow::OnLightTheme);

        // Help actions
        connect(m_aboutAction, &QAction::triggered, this, &MainWindow::OnAbout);
        connect(m_aboutQtAction, &QAction::triggered, this, &MainWindow::OnAboutQt);

        // Viewer signals
        connect(m_viewer, &QtOccView::ShapeSelected, this, &MainWindow::OnShapeSelected);
        connect(m_viewer, &QtOccView::ViewChanged, this, &MainWindow::OnViewChanged);
        connect(m_viewer, &QtOccView::FaceSelected, this, &MainWindow::OnFaceSelected);
        connect(m_viewer, &QtOccView::SketchModeEntered, this, &MainWindow::OnSketchModeEntered);
        connect(m_viewer, &QtOccView::SketchModeExited, this, &MainWindow::OnSketchModeExited);
        connect(m_viewer, &QtOccView::SketchHistoryChanged, this, &MainWindow::UpdateActions);
        connect(m_viewer, &QtOccView::SketchToolChanged, this, &MainWindow::OnSketchToolChanged);

        // Document tree signals for selection synchronization
        connect(m_documentTree, &DocumentTree::ShapeSelected, this, &MainWindow::OnDocumentTreeShapeSelected);
        connect(m_documentTree, &DocumentTree::FeatureSelected, this, &MainWindow::OnDocumentTreeFeatureSelected);
        connect(m_documentTree, &DocumentTree::ShapeDeleted, this, &MainWindow::OnDocumentTreeShapeDeleted);
        connect(m_documentTree, &DocumentTree::FeatureDeleted, this, &MainWindow::OnDocumentTreeFeatureDeleted);
        connect(m_documentTree, &DocumentTree::SketchDeleted, this, &MainWindow::OnDocumentTreeSketchDeleted);
        connect(m_documentTree, &DocumentTree::SketchEditRequested, this, &MainWindow::OnEditSketchRequested);
        connect(m_documentTree, &DocumentTree::ShapeVisibilityChanged, this, &MainWindow::OnDocumentTreeShapeVisibilityChanged);
        connect(m_documentTree, &DocumentTree::SketchVisibilityChanged, this, &MainWindow::OnDocumentTreeSketchVisibilityChanged);

        // Listen for modification signals from the property panel

        connect(m_propertyPanel, &PropertyPanel::FeatureParameterChanged,
            this, &MainWindow::OnFeatureParameterChanged);
    }

    void MainWindow::UpdateActions() {
        bool canUndo = false;
        bool canRedo = false;

        // If sketch mode is active, read the sketch history first
        if (m_viewer && m_viewer->IsInSketchMode()) {
            canUndo = m_viewer->CanUndoSketch();
            canRedo = m_viewer->CanRedoSketch();
        }
        // Read OCAF 3D history only when not in sketch mode
        else {
            canUndo = m_commandManager->CanUndo();
            canRedo = m_commandManager->CanRedo();
        }

        m_saveAction->setEnabled(m_documentModified);
        m_saveAsAction->setEnabled(true); 

        m_undoAction->setEnabled(canUndo);
        m_redoAction->setEnabled(canRedo);

        // Update action text based on availability
        m_undoAction->setText(canUndo ? "&Undo" : "&Undo");
        m_redoAction->setText(canRedo ? "&Redo" : "&Redo");
    }

    void MainWindow::RefreshUIFromOCAF() {
        if (!m_ocafManager) return;

        // 1. Clear the current UI

        m_viewer->ClearShapes();
        m_documentTree->Clear(); // Note: Clear() does not remove the Sketches tree by default


        // 2. Get all currently alive 3D entities from OCAF

        auto allShapes = m_ocafManager->GetAllShapes();

        // 3. Dynamically resolve the feature dependency tree to find active features

        std::vector<cad_feature::FeaturePtr> activeFeatures;
        std::vector<cad_core::ShapePtr> absorbedShapes; // Shapes absorbed and hidden by features


        if (m_featureManager) {
            for (const auto& feature : m_featureManager->GetFeatures()) {
                auto resultShape = feature->GetResultShape();
                bool isFeatureAlive = false;

                // Check whether the output of this feature still exists in OCAF

                // Use OCC IsSame to compare the underlying topology precisely

                if (resultShape) {
                    for (const auto& s : allShapes) {
                        if (s && s->GetOCCTShape().IsSame(resultShape->GetOCCTShape())) {
                            isFeatureAlive = true;
                            break;
                        }
                    }
                }

                if (isFeatureAlive) {
                    feature->SetActive(true);
                    activeFeatures.push_back(feature);
                    m_documentTree->AddFeature(feature); // The feature is still alive, so reattach it to the UI tree


                    // Collect input shapes consumed by this active feature

                    auto boolFeat = std::dynamic_pointer_cast<cad_feature::BooleanFeature>(feature);
                    if (boolFeat) {
                        for (const auto& t : boolFeat->GetTargets()) absorbedShapes.push_back(t);
                        for (const auto& t : boolFeat->GetTools()) absorbedShapes.push_back(t);
                    }
                    auto fcFeat = std::dynamic_pointer_cast<cad_feature::FilletChamferFeature>(feature);
                    if (fcFeat && fcFeat->GetBaseShape()) {
                        absorbedShapes.push_back(fcFeat->GetBaseShape());
                    }
                }
                else {
                    // If the feature output is gone (e.g. deleted or undone), mark the feature as inactive

                    feature->SetActive(false);
                }
            }
        }

        // 4. Restore shapes to the view and document tree

        for (const auto& shape : allShapes) {
            if (shape) {
                bool isAbsorbed = false;
                for (const auto& absorbed : absorbedShapes) {
                    if (absorbed && absorbed->GetOCCTShape().IsSame(shape->GetOCCTShape())) {
                        isAbsorbed = true;
                        break;
                    }
                }

                // Only shapes that were not absorbed remain visible final bodies

                if (!isAbsorbed) {
                    m_viewer->DisplayShape(shape);
                    m_documentTree->AddShape(shape);
                }
            }
        }

        m_viewer->ClearSelection();
        m_viewer->ClearEdgeSelection();
        m_viewer->RedrawAll();
    }

    void MainWindow::UpdateWindowTitle() {
        QString title = "JLi CAD";
        if (!m_currentFileName.isEmpty()) {
            title += " - " + QFileInfo(m_currentFileName).baseName();
            if (m_documentModified) {
                title += " *";
            }
        }
        setWindowTitle(title);
    }

    void MainWindow::closeEvent(QCloseEvent* event) {
        if (SaveChanges()) {
            event->accept();
        }
        else {
            event->ignore();
        }
    }

    bool MainWindow::SaveChanges() {
        if (m_documentModified) {
            QMessageBox::StandardButton result = QMessageBox::question(this,
                "Save Changes",
                "The document has been modified. Do you want to save your changes?",
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

            if (result == QMessageBox::Save) {
                return OnSaveDocument();
            }
            else if (result == QMessageBox::Cancel) {
                return false;
            }
        }
        return true;
    }

    void MainWindow::SetDocumentModified(bool modified) {
        m_documentModified = modified;
        UpdateActions();
        UpdateWindowTitle();
    }

    // Slot implementations
    void MainWindow::OnNewDocument() {
        if (!SaveChanges()) return; 

		// 1. clear current UI
        m_viewer->ClearShapes();
        m_documentTree->Clear();

		// 2. reset OCAF Document and internal managers
        m_featureManager = std::make_unique<cad_feature::FeatureManager>();
        m_commandManager = std::make_unique<cad_core::CommandManager>();

        // 3. New OCAF Document
        m_ocafManager->NewDocument();

		// 4. reset
        m_currentFileName.clear();
        SetDocumentModified(false);
        m_viewer->FitAll();
        m_viewer->RedrawAll();

        statusBar()->showMessage("New document created", 2000);
    }

    void MainWindow::OnOpenDocument() {
        if (!SaveChanges()) return; 

        QString fileName = QFileDialog::getOpenFileName(this, tr("Open Document"), "", tr("CAD Files (*.cad)"));
        if (fileName.isEmpty()) return;

		// call the underlying OCAF manager to open the document. 
        if (m_ocafManager->OpenDocument(fileName.toStdString())) {
            m_currentFileName = fileName;
            SetDocumentModified(false);

            RefreshUIFromOCAF();

            UpdateWindowTitle();
            statusBar()->showMessage(tr("Document loaded."), 2000);
        }
        else {
            QMessageBox::critical(this, tr("Error"), tr("Could not open file."));
        }
    }

    bool MainWindow::OnSaveDocument() {
        if (m_currentFileName.isEmpty()) {
            return OnSaveDocumentAs();
        }

		// call the underlying OCAF manager to save the documen
        if (m_ocafManager->SaveDocument(m_currentFileName.toStdString())) {
            SetDocumentModified(false);
            statusBar()->showMessage(tr("Document saved."), 2000);
            return true;
        }
        else {
            QMessageBox::critical(this, tr("Error"), tr("Failed to save document."));
            return false;
        }
    }

    bool MainWindow::OnSaveDocumentAs() {
        QString fileName = QFileDialog::getSaveFileName(this, tr("Save Document"), "", tr("CAD Files (*.cad)"));
        if (fileName.isEmpty()) return false;

		// make sure the file name has the correct extension
        if (!fileName.endsWith(".cad", Qt::CaseInsensitive)) {
            fileName += ".cad";
        }

        if (m_ocafManager->SaveDocument(fileName.toStdString())) {
            m_currentFileName = fileName;
            SetDocumentModified(false);
            UpdateWindowTitle();
            statusBar()->showMessage(tr("Document saved as %1").arg(fileName), 2000);
            return true;
        }
        return false;
    }

    void MainWindow::OnExit() {
        close();
    }

    void MainWindow::OnUndo() {
        if (m_viewer && m_viewer->IsInSketchMode()) {
            m_viewer->UndoSketch();
            return;
        }
        if (m_commandManager->Undo()) {
            RefreshUIFromOCAF();
            SetDocumentModified(true);
            UpdateActions();
            statusBar()->showMessage("Undo completed", 2000);
        }
        else {
            statusBar()->showMessage("Cannot undo", 2000);
        }
    }

    void MainWindow::OnRedo() {
        if (m_viewer && m_viewer->IsInSketchMode()) {
            m_viewer->RedoSketch();
            return;
        }
        if (m_commandManager->Redo()) {
            RefreshUIFromOCAF();
            SetDocumentModified(true);
            UpdateActions();
            statusBar()->showMessage("Redo completed", 2000);
        }
        else {
            statusBar()->showMessage("Cannot redo", 2000);
        }
    }

    void MainWindow::OnFitAll() {
        m_viewer->FitAll();
        m_viewer->RedrawAll();
    }

    void MainWindow::OnZoomIn() {
        m_viewer->ZoomIn();
    }

    void MainWindow::OnZoomOut() {
        m_viewer->ZoomOut();
    }

    void MainWindow::OnViewWireframe() {
        m_viewer->SetViewMode("wireframe");
    }

    void MainWindow::OnViewShaded() {
        m_viewer->SetViewMode("shaded");
    }

    void MainWindow::OnViewOrthographic() {
        m_viewer->SetProjectionMode(true);
    }

    void MainWindow::OnViewPerspective() {
        m_viewer->SetProjectionMode(false);
    }

    void MainWindow::OnSetTransparency() {
        if (m_viewer) {
            m_viewer->SetAllTransparency(0.5); // Set to 50% transparency
        }
    }

    void MainWindow::OnSetShapeTransparency() {
        if (!m_viewer) return;

        // 1. Get the currently selected object

        cad_core::ShapePtr selectedShape = m_viewer->GetCurrentSelectedShape();

        if (!selectedShape) {
            // Use an English prompt in the UI when nothing is selected

            QMessageBox::information(this, "Information", "Please select a shape in the view first.");
            return;
        }

        // 2. Show a dialog to let the user enter transparency

        bool ok;
        double transparency = QInputDialog::getDouble(this,
            "Set Transparency",
            "Enter transparency (0.0 = opaque, 1.0 = fully transparent):",
            0.5,   // Default value
            0.0,   // Min value
            1.0,   // Max value
            2,     // Decimals
            &ok);

        // 3. Apply the transparency if the user confirms

        if (ok) {
            m_viewer->SetShapeTransparency(selectedShape, transparency);
            statusBar()->showMessage(QString("Transparency set to %1").arg(transparency), 2000);
        }
    }

    void MainWindow::OnCreateFace() {
        m_ocafManager->StartTransaction("Create Planar Face");

        try {
            // 1. Instantiate the parametric feature

            std::string featureName = "PlaneFace_" + std::to_string(m_featureManager->GetFeatureCount() + 1);
            auto faceFeature = std::make_shared<cad_feature::RectangularFaceFeature>(featureName);

            // Use the default size 10x10

            faceFeature->SetWidth(10.0);
            faceFeature->SetHeight(10.0);

            // 2. Let the feature generate the 3D shape

            auto resultShape = faceFeature->CreateShape();

            if (resultShape) {
                // Bind the generated shape to the feature; this step is critical

                faceFeature->SetResultShape(resultShape);

                if (m_ocafManager->AddShape(resultShape, featureName)) {
                    // 3. Register the feature in the backend manager and the document tree

                    m_featureManager->AddFeature(faceFeature);
                    m_documentTree->AddFeature(faceFeature);

                    // Display the final shape

                    m_viewer->DisplayShape(resultShape);
                    m_documentTree->AddShape(resultShape);

                    m_ocafManager->CommitTransaction();
                    m_commandManager->ExecuteCommand(std::make_shared<cad_core::OCAFTransactionCommand>(m_ocafManager.get(), "Create Planar Face"));
          
                    SetDocumentModified(true);
                    SetDocumentModified(true);
                    UpdateActions();
                    statusBar()->showMessage("Planar face feature created.");
                }
                else {
                    throw std::runtime_error("Failed to add shape to document.");
                }
            }
            else {
                throw std::runtime_error("Failed to create face shape.");
            }
        }
        catch (const std::exception& e) {
            m_ocafManager->AbortTransaction();
            QMessageBox::warning(this, "Error", e.what());
        }
    }

    void MainWindow::OnCreateBox() {
        CreateBoxDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            double width = dialog.GetWidth();
            double height = dialog.GetHeight();
            double depth = dialog.GetDepth();

            auto cmd = std::make_shared<cad_core::CreateBoxCommand>(
                width, height, depth, m_ocafManager.get());

            if (m_commandManager->ExecuteCommand(cmd)) {
                auto shape = cmd->GetCreatedShape();
                m_viewer->DisplayShape(shape);
                m_documentTree->AddShape(shape);
                SetDocumentModified(true);
                UpdateActions();
            }
            else {
                QMessageBox::warning(this, "Error", "Failed to create box. Check parameters.");
            }
        }
    }

    void MainWindow::OnCreateCylinder() {
        CreateCylinderDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            double radius = dialog.GetRadius();
            double height = dialog.GetHeight();

            auto cmd = std::make_shared<cad_core::CreateCylinderCommand>(
                radius, height, m_ocafManager.get());

            if (m_commandManager->ExecuteCommand(cmd)) {
                auto shape = cmd->GetCreatedShape();
                m_viewer->DisplayShape(shape);
                m_documentTree->AddShape(shape);
                SetDocumentModified(true);
                UpdateActions();
            }
            else {
                QMessageBox::warning(this, "Error", "Failed to create cylinder. Check parameters.");
            }
        }
    }

    void MainWindow::OnCreateSphere() {
        CreateSphereDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            double radius = dialog.GetRadius();

            auto cmd = std::make_shared<cad_core::CreateSphereCommand>(
                radius, m_ocafManager.get());

            if (m_commandManager->ExecuteCommand(cmd)) {
                auto shape = cmd->GetCreatedShape();
                m_viewer->DisplayShape(shape);
                m_documentTree->AddShape(shape);
                SetDocumentModified(true);
                UpdateActions();
            }
            else {
                QMessageBox::warning(this, "Error", "Failed to create sphere. Check parameters.");
            }
        }
    }

    void MainWindow::OnCreateExtrude() {
        // If the dialog is already open, just activate it

        if (m_currentExtrudeDialog) {
            m_currentExtrudeDialog->show();
            m_currentExtrudeDialog->raise();
            m_currentExtrudeDialog->activateWindow();
            return;
        }

        // Open the extrude dialog directly instead of using the old base-face workflow

        m_currentExtrudeDialog = new CreateExtrudeDialog(this);
        connect(m_currentExtrudeDialog, &CreateExtrudeDialog::extrudeRequested,
            this, &MainWindow::OnExtrudeRequested);
        connect(m_currentExtrudeDialog, &CreateExtrudeDialog::previewRequested, this, &MainWindow::OnExtrudePreviewRequested);
        connect(m_currentExtrudeDialog, &QDialog::rejected, this, &MainWindow::ClearPreview);
        connect(m_currentExtrudeDialog, &QDialog::finished,
            this, &MainWindow::OnExtrudeDialogClosed);

        // Do not force face selection mode; keep normal viewer interaction

        m_currentExtrudeDialog->show();
        m_currentExtrudeDialog->raise();
        m_currentExtrudeDialog->activateWindow();

        statusBar()->showMessage("Extrude dialog opened. Please select a profile or face.");
    }

    void MainWindow::OnExtrudeRequested(cad_core::ShapePtr baseShape, double distance) {
        ClearPreview();
        if (!baseShape) return;

        m_ocafManager->StartTransaction("Create Extrude Feature");

        try {
            // Create and configure the feature

            // Generate names such as "Extrude_1" and "Extrude_2"

            std::string featureName = "Extrude_" + std::to_string(m_featureManager->GetFeatureCount() + 1);
            auto extrudeFeature = std::make_shared<cad_feature::ExtrudeFeature>(featureName);

            // Assign feature parameters

            extrudeFeature->SetProfileShape(baseShape);
            extrudeFeature->SetDistance(distance);

            // Let the feature run its recipe and generate the real 3D shape

            auto resultShape = extrudeFeature->CreateShape();

            if (resultShape) {
                extrudeFeature->SetResultShape(resultShape);
                // 1. Store the 3D result in the underlying data structure

                if (m_ocafManager->AddShape(resultShape, featureName)) {

                    // 2. Register the feature in the feature manager and document tree

                    m_featureManager->AddFeature(extrudeFeature);
                    m_documentTree->AddFeature(extrudeFeature);
                    // Note: DocumentTree distinguishes between Shapes and Features,

                    // so an "Extrude_1" node will appear under Features in the tree


                    // 3. Show the result shape in the document tree and 3D view

                    m_documentTree->AddShape(resultShape);
                    m_viewer->DisplayShape(resultShape);

                    // The feature consumes the sketch

                    // 4. Find which sketch owns the face used for this extrusion

                    std::shared_ptr<cad_sketch::Sketch> targetSketch = nullptr;
                    for (const auto& sketch : m_documentTree->GetAllSketches()) {
                        for (const auto& profile : sketch->GetProfiles()) {
                            // Use OCC IsSame to compare underlying faces precisely

                            if (profile->GetFace().IsSame(baseShape->GetOCCTShape())) {
                                targetSketch = sketch;
                                break;
                            }
                        }
                        if (targetSketch) break;
                    }

                    // Hide it in the 3D view and mark it with a strikeout in the tree

                    if (targetSketch) {
                        m_viewer->SetSketchVisibility(targetSketch, false);
                        m_documentTree->SetSketchUIHidden(targetSketch, true);
                    }

                    m_viewer->ClearSelection();

                    m_ocafManager->CommitTransaction();

                    m_commandManager->ExecuteCommand(
                        std::make_shared<cad_core::OCAFTransactionCommand>(m_ocafManager.get(), "Create Extrude Feature"));

                    SetDocumentModified(true);
                    UpdateActions();
                }
                else {
                    throw std::runtime_error("Failed to add shape to document.");
                }
            }
            else {
                throw std::runtime_error("Extrude feature failed to generate shape.");
            }
        }
        catch (const std::exception& e) {
            m_ocafManager->AbortTransaction();
            QMessageBox::warning(this, "Extrude Error", e.what());
        }
    }

    // Process extrude preview logic
    void MainWindow::OnExtrudePreviewRequested(cad_core::ShapePtr baseShape, double distance) {
        ClearPreview(); // 先清理旧的
        if (!baseShape) return;

        try {
            auto previewFeature = std::make_shared<cad_feature::ExtrudeFeature>("Preview");
            previewFeature->SetProfileShape(baseShape);
            previewFeature->SetDistance(distance);

            auto resultShape = previewFeature->CreateShape();
            if (resultShape) {
                m_previewShapes.push_back(resultShape);
                m_previewActive = true;
                m_viewer->DisplayShape(resultShape);
                m_viewer->SetShapeTransparency(resultShape, 0.5); 
                m_viewer->update();
            }
        }
        catch (...) {}
    }

    void MainWindow::OnExtrudeDialogClosed() {
        m_currentExtrudeDialog = nullptr;

        // Restore the normal status prompt when the extrude dialog closes

        statusBar()->showMessage("Extrude dialog closed.");
    }

    void MainWindow::OnSweepRequested(cad_core::ShapePtr profileShape, cad_core::ShapePtr pathShape, double twistAngle, double scaleFactor, bool keepOrientation) {
        ClearPreview();
        
        if (!profileShape || !pathShape) return;

        // Start an OCAF history transaction

        m_ocafManager->StartTransaction("Create Sweep Feature");

        try {
            // 1. Create the sweep feature

            std::string featureName = "Sweep_" + std::to_string(m_featureManager->GetFeatureCount() + 1);
            auto sweepFeature = std::make_shared<cad_feature::SweepFeature>(featureName);

            sweepFeature->SetProfileShape(profileShape);
            sweepFeature->SetPathShape(pathShape);
            sweepFeature->SetTwistAngle(twistAngle);
            sweepFeature->SetScaleFactor(scaleFactor);
            sweepFeature->SetKeepOriginalOrientation(keepOrientation);

            // 2. Generate the 3D shape

            auto resultShape = sweepFeature->CreateShape();

            if (resultShape) {
                sweepFeature->SetResultShape(resultShape);

                // 3. Write the result into the underlying OCAF database

                if (m_ocafManager->AddShape(resultShape, featureName)) {

                    // 4. Add the feature to the tree

                    m_featureManager->AddFeature(sweepFeature);
                    m_documentTree->AddFeature(sweepFeature);  // A Sweep node appears in the left DocumentTree


                    // Display the final 3D entity

                    m_documentTree->AddShape(resultShape);
                    m_viewer->DisplayShape(resultShape);

                    // 5. Remove the sketch lines used to draw the path

                    m_viewer->CleanupSweepUI();
                    m_viewer->ClearSelection();

                    m_ocafManager->CommitTransaction();
                    m_commandManager->ExecuteCommand(
                        std::make_shared<cad_core::OCAFTransactionCommand>(m_ocafManager.get(), "Create Sweep Feature"));
                    SetDocumentModified(true);
                    UpdateActions();
                    statusBar()->showMessage("Sweep completed successfully");
                }
                else {
                    throw std::runtime_error("Failed to add shape to document.");
                }
            }
            else {
                throw std::runtime_error("Sweep feature failed to generate shape.");
            }
        }
        catch (const std::exception& e) {
            m_ocafManager->AbortTransaction();
            m_viewer->CancelSweepInteraction(); // Rollback on failure

            QMessageBox::warning(this, "Sweep Error", e.what());
        }
    }

	// process sweep preview logic
    void MainWindow::OnSweepPreviewRequested(cad_core::ShapePtr profile, cad_core::ShapePtr path, double twist, double scale, bool keepOrientation) {
        ClearPreview();
        if (!profile || !path) return;

        try {
            auto previewFeature = std::make_shared<cad_feature::SweepFeature>("Preview");
            previewFeature->SetProfileShape(profile);
            previewFeature->SetPathShape(path);
            previewFeature->SetTwistAngle(twist);
            previewFeature->SetScaleFactor(scale);
            previewFeature->SetKeepOriginalOrientation(keepOrientation);

            auto resultShape = previewFeature->CreateShape();
            if (resultShape) {
                m_previewShapes.push_back(resultShape);
                m_previewActive = true;
                m_viewer->DisplayShape(resultShape);
                m_viewer->SetShapeTransparency(resultShape, 0.5);
                m_viewer->update();
            }
        }
        catch (...) {}
    }


    void MainWindow::OnDarkTheme() {
        m_themeManager->SetTheme("dark");
    }

    void MainWindow::OnLightTheme() {
        m_themeManager->SetTheme("light");
    }

    void MainWindow::OnAbout() {
        AboutDialog dialog(this);
        dialog.exec();
    }

    void MainWindow::OnAboutQt() {
        QMessageBox::aboutQt(this);
    }

    void MainWindow::OnShapeSelected(const cad_core::ShapePtr& shape) {
        // Update property panel with selected shape
        m_propertyPanel->SetShape(shape);

        // Forward selection to active dialogs
        OnObjectSelected(shape);
    }

    void MainWindow::OnViewChanged() {
        // Handle view changes
    }

    // Document tree selection handlers
    void MainWindow::OnDocumentTreeShapeSelected(const cad_core::ShapePtr& shape) {
        // When a shape is selected in the document tree, select it in the 3D viewer
        if (m_viewer && shape) {
            m_viewer->SelectShape(shape);
            m_propertyPanel->SetShape(shape);
            OnObjectSelected(shape);
        }
    }

    void MainWindow::OnDocumentTreeFeatureSelected(const cad_feature::FeaturePtr& feature) {
        if (!feature) return;

        m_propertyPanel->SetFeature(feature);
        qDebug() << "Feature selected:" << QString::fromStdString(feature->GetName());
    }

    void MainWindow::OnDocumentTreeShapeDeleted(const cad_core::ShapePtr& shape) {
        if (!shape) return;

        // Start a history transaction so deletion can also be undone

        m_ocafManager->StartTransaction("Delete Shape");

        try {
            // 1. Delete the data from the underlying OCAF document

            if (m_ocafManager->RemoveShape(shape)) {
                // 2. Erase the object from the 3D view

                m_viewer->RemoveShape(shape);

                // Commit the history transaction

                m_ocafManager->CommitTransaction();
                m_commandManager->ExecuteCommand(
                    std::make_shared<cad_core::OCAFTransactionCommand>(m_ocafManager.get(), "Delete Shape"));
                SetDocumentModified(true);
                UpdateActions();

                // If the deleted object was selected, clear the property panel and selection state

                m_viewer->ClearSelection();

                statusBar()->showMessage("Shape deleted successfully", 2000);
            }
            else {
                m_ocafManager->AbortTransaction();
                QMessageBox::warning(this, "Error", "Failed to delete shape from OCAF document.");
            }
        }
        catch (const std::exception& e) {
            m_ocafManager->AbortTransaction();
            QMessageBox::critical(this, "Error", QString("Exception during deletion: %1").arg(e.what()));
        }
    }

    void MainWindow::OnDocumentTreeSketchDeleted(const std::shared_ptr<cad_sketch::Sketch>& sketch) {
        if (!sketch || !m_viewer) return;

        try {
            // 1. Remove all graphics and profiles of this sketch from the view

            m_viewer->RemoveSketch(sketch);

            // 2. If this is the active sketch, clear its content

            auto activeSketch = m_viewer->GetActiveSketch();
            if (activeSketch == sketch) {
                sketch->ClearElements();
                sketch->UpdateProfiles(gp_Ax3());
            }

            // 3. Clear the selection state and UI

            m_viewer->ClearSelection();
            SetDocumentModified(true);
            UpdateActions();
            m_viewer->RedrawAll();
            statusBar()->showMessage("Sketch deleted successfully", 2000);
        }
        catch (const std::exception& e) {
            QMessageBox::critical(this, "Error", QString("Exception during sketch deletion: %1").arg(e.what()));
        }
    }


    void MainWindow::OnDocumentTreeFeatureDeleted(const cad_feature::FeaturePtr& feature) {
        if (!feature) return;

        // Start an OCAF transaction so feature deletion is recorded and can be undone

        m_ocafManager->StartTransaction("Delete Feature");

        try {
            auto resultShape = feature->GetResultShape();
            if (resultShape) {
                // Only remove the 3D body from OCAF and the view

                m_ocafManager->RemoveShape(resultShape);
            }

            m_ocafManager->CommitTransaction();
            m_commandManager->ExecuteCommand(
                std::make_shared<cad_core::OCAFTransactionCommand>(m_ocafManager.get(), "Delete Feature"));
            SetDocumentModified(true);
            UpdateActions();

            // Trigger a global state refresh so the UI can determine which old entities to release

            RefreshUIFromOCAF();

            statusBar()->showMessage(QString("Feature '%1' deleted successfully").arg(QString::fromStdString(feature->GetName())), 2000);
        }
        catch (const std::exception& e) {
            m_ocafManager->AbortTransaction();
            QMessageBox::critical(this, "Error", QString("Exception during feature deletion: %1").arg(e.what()));
        }
    }
    void MainWindow::OnDocumentTreeShapeVisibilityChanged(const cad_core::ShapePtr& shape, bool visible) {
        if (m_viewer && shape) {
            m_viewer->SetShapeVisibility(shape, visible);

            // Show a hint in the status bar

            if (visible) {
                statusBar()->showMessage("Shape is now visible", 2000);
            }
            else {
                statusBar()->showMessage("Shape is now hidden", 2000);
            }
        }
    }

    void MainWindow::OnDocumentTreeSketchVisibilityChanged(const std::shared_ptr<cad_sketch::Sketch>& sketch, bool visible) {
        if (m_viewer && sketch) {
            m_viewer->SetSketchVisibility(sketch, visible);
            statusBar()->showMessage(visible ? "Sketch is now visible" : "Sketch is now hidden", 2000);
        }
    }

    // Missing slot implementations
    void MainWindow::OnCut() {
        // Cut implementation placeholder
    }

    void MainWindow::OnCopy() {
        // Copy implementation placeholder
    }

    void MainWindow::OnPaste() {
        // Paste implementation placeholder
    }

    void MainWindow::OnDelete() {
        // Delete implementation placeholder
    }

    void MainWindow::OnSelectAll() {
        // Select all implementation placeholder
    }

    void MainWindow::OnCreateRevolve() {
        if (m_currentRevolveDialog) {
            m_currentRevolveDialog->activateWindow();
            return;
        }

        m_currentRevolveDialog = new CreateRevolveDialog(this);

        connect(m_currentRevolveDialog, &CreateRevolveDialog::revolveRequested,
            this, &MainWindow::OnRevolveRequested);
        connect(m_currentRevolveDialog, &QDialog::finished,
            this, &MainWindow::OnRevolveDialogClosed);
        connect(m_currentRevolveDialog, &CreateRevolveDialog::previewRequested, this, &MainWindow::OnRevolvePreviewRequested);
        connect(m_currentRevolveDialog, &QDialog::rejected, this, &MainWindow::ClearPreview);

        m_currentRevolveDialog->show();
    }

    void MainWindow::OnRevolveRequested(cad_core::ShapePtr baseShape, double angle,
        double axOriginX, double axOriginY, double axOriginZ,
        double axDirX, double axDirY, double axDirZ)
    {
        ClearPreview();
        if (!baseShape) return;

        m_ocafManager->StartTransaction("Create Revolve Feature");

        try {
            std::string featureName = "Revolve_" + std::to_string(m_featureManager->GetFeatureCount() + 1);
            auto revolveFeature = std::make_shared<cad_feature::RevolveFeature>(featureName);

            revolveFeature->SetProfileShape(baseShape);
            revolveFeature->SetAngle(angle);
            revolveFeature->SetAxisOrigin(axOriginX, axOriginY, axOriginZ);
            revolveFeature->SetAxis(axDirX, axDirY, axDirZ);

            auto resultShape = revolveFeature->CreateShape();

            if (resultShape) {
                revolveFeature->SetResultShape(resultShape);

                if (m_ocafManager->AddShape(resultShape, featureName)) {
                    m_featureManager->AddFeature(revolveFeature);
                    m_documentTree->AddFeature(revolveFeature);
                    m_documentTree->AddShape(resultShape);
                    m_viewer->DisplayShape(resultShape);

					// hide the original profile face and its sketch
                    std::shared_ptr<cad_sketch::Sketch> targetSketch = nullptr;
                    for (const auto& sketch : m_documentTree->GetAllSketches()) {
                        for (const auto& profile : sketch->GetProfiles()) {
                            if (profile->GetFace().IsSame(baseShape->GetOCCTShape())) {
                                targetSketch = sketch;
                                break;
                            }
                        }
                        if (targetSketch) break;
                    }
                    if (targetSketch) {
                        m_viewer->SetSketchVisibility(targetSketch, false);
                        m_documentTree->SetSketchUIHidden(targetSketch, true);
                    }

                    m_viewer->ClearSelection();

                    m_ocafManager->CommitTransaction();
                    m_commandManager->ExecuteCommand(
                        std::make_shared<cad_core::OCAFTransactionCommand>(m_ocafManager.get(), "Create Revolve Feature"));
                    SetDocumentModified(true);
                    UpdateActions();
                }
                else {
                    throw std::runtime_error("Failed to add shape to document.");
                }
            }
            else {
                throw std::runtime_error("Revolve feature failed to generate shape.");
            }
        }
        catch (const std::exception& e) {
            m_ocafManager->AbortTransaction();
            QMessageBox::warning(this, "Revolve Error", e.what());
        }
    }

    void MainWindow::OnRevolvePreviewRequested(cad_core::ShapePtr baseShape, double angle,
        double axOriginX, double axOriginY, double axOriginZ,
        double axDirX, double axDirY, double axDirZ)
    {
        ClearPreview();
        if (!baseShape) return;

        try {
            auto previewFeature = std::make_shared<cad_feature::RevolveFeature>("Preview");
            previewFeature->SetProfileShape(baseShape);
            previewFeature->SetAngle(angle);
            previewFeature->SetAxisOrigin(axOriginX, axOriginY, axOriginZ);
            previewFeature->SetAxis(axDirX, axDirY, axDirZ);

            auto resultShape = previewFeature->CreateShape();
            if (resultShape) {
                m_previewShapes.push_back(resultShape);
                m_previewActive = true;
                m_viewer->DisplayShape(resultShape);
                m_viewer->SetShapeTransparency(resultShape, 0.5);
                m_viewer->update();
            }
        }
        catch (...) {}
    }

    void MainWindow::OnRevolveDialogClosed() {
        m_currentRevolveDialog = nullptr;
    }

    void MainWindow::OnCreateSweep() {
        auto sweepDialog = new cad_ui::CreateSweepDialog(m_viewer, this);
        sweepDialog->setAttribute(Qt::WA_DeleteOnClose);

        m_viewer->StartSweepInteraction();

        connect(sweepDialog, &cad_ui::CreateSweepDialog::sweepRequested,
            this, &MainWindow::OnSweepRequested);
        connect(sweepDialog, &cad_ui::CreateSweepDialog::previewRequested,
            this, &MainWindow::OnSweepPreviewRequested);
        connect(sweepDialog, &QDialog::rejected, this, &MainWindow::ClearPreview);

        sweepDialog->show();
        statusBar()->showMessage("Sweep feature activated. Follow the instructions on the panel.");
    }

    void MainWindow::OnCreateLoft() {
        // Avoid opening it repeatedly

        if (m_currentLoftDialog) {
            m_currentLoftDialog->show();
            m_currentLoftDialog->raise();
            m_currentLoftDialog->activateWindow();
            return;
        }

        m_currentLoftDialog = new CreateLoftDialog(m_viewer, this);

        connect(m_currentLoftDialog, &CreateLoftDialog::loftRequested,
            this, &MainWindow::OnLoftRequested);
        connect(m_currentLoftDialog, &CreateLoftDialog::previewRequested,
            this, &MainWindow::OnLoftPreviewRequested);

        // When the dialog closes, restore the default selection mode and reset the pointer

        connect(m_currentLoftDialog, &QDialog::finished, this, [this]() {
            ClearPreview();
            m_currentLoftDialog = nullptr;
            m_viewer->SetSelectionMode(0); // Restore Shape selection mode

            statusBar()->showMessage("Loft dialog closed.");
            });

        m_currentLoftDialog->show();
        statusBar()->showMessage("Loft feature activated. Please select profiles sequentially in the 3D view.");
    }

    void MainWindow::OnLoftRequested(const std::vector<cad_core::ShapePtr>& sections, bool isSolid) {
		ClearPreview();
        if (sections.size() < 2) return;

        // Start history tracking to support undo

        m_ocafManager->StartTransaction("Create Loft Feature");

        try {
            // 1. Initialize the feature

            std::string featureName = "Loft_" + std::to_string(m_featureManager->GetFeatureCount() + 1);
            auto loftFeature = std::make_shared<cad_feature::LoftFeature>(featureName);

            loftFeature->SetSolid(isSolid);
            for (const auto& sec : sections) {
                loftFeature->AddSection(sec);
            }

            // Comment translated to English
            auto resultShape = loftFeature->CreateShape();

            if (resultShape) {
                loftFeature->SetResultShape(resultShape);

                // 3. Add the new shape to the OCAF data core

                if (m_ocafManager->AddShape(resultShape, featureName)) {

                    // 4. Update the document tree and 3D view

                    m_featureManager->AddFeature(loftFeature);
                    m_documentTree->AddFeature(loftFeature);

                    m_documentTree->AddShape(resultShape);
                    m_viewer->DisplayShape(resultShape);

                    // 5. Hide or remove the original profile face

                    for (const auto& sec : sections) {
                        // Hide a normal 3D face or body

                        m_viewer->SetShapeVisibility(sec, false);
                        m_documentTree->RemoveShape(sec);

                        // Find the sketch that owns the consumed face, then fully hide it in the tree and view

                        std::shared_ptr<cad_sketch::Sketch> targetSketch = nullptr;
                        for (const auto& sketch : m_documentTree->GetAllSketches()) {
                            for (const auto& profile : sketch->GetProfiles()) {
                                // Comment translated to English
                                if (profile->GetFace().IsSame(sec->GetOCCTShape())) {
                                    targetSketch = sketch;
                                    break;
                                }
                            }
                            if (targetSketch) break;
                        }

                        // Hide the sketch graphics in the view and add a gray hidden mark in the tree

                        if (targetSketch) {
                            m_viewer->SetSketchVisibility(targetSketch, false);
                            m_documentTree->SetSketchUIHidden(targetSketch, true);
                        }
                    }

                    m_viewer->ClearSelection();

                    m_ocafManager->CommitTransaction();
                    m_commandManager->ExecuteCommand(
                        std::make_shared<cad_core::OCAFTransactionCommand>(m_ocafManager.get(), "Create Loft Feature"));
                    SetDocumentModified(true);
                    UpdateActions();

                    statusBar()->showMessage("Loft feature generated successfully.");
                }
                else {
                    throw std::runtime_error("Failed to add Loft shape to document.");
                }
            }
            else {
                throw std::runtime_error("Loft algorithm failed to generate shape. Please check if the selected profiles are valid and correctly oriented.");
            }
        }
        catch (const std::exception& e) {
            m_ocafManager->AbortTransaction();
            QMessageBox::warning(this, "Loft Error", e.what());
        }
    }

	// loft preview logic
    void MainWindow::OnLoftPreviewRequested(const std::vector<cad_core::ShapePtr>& sections, bool isSolid) {
        ClearPreview();
		if (sections.size() < 2) return; // if less than 2 sections, loft cannot be generated, so skip preview

        try {
            auto previewFeature = std::make_shared<cad_feature::LoftFeature>("Preview");
            previewFeature->SetSolid(isSolid);
            for (const auto& sec : sections) {
                previewFeature->AddSection(sec);
            }

            auto resultShape = previewFeature->CreateShape();
            if (resultShape) {
                m_previewShapes.push_back(resultShape);
                m_previewActive = true;
                m_viewer->DisplayShape(resultShape);
                m_viewer->SetShapeTransparency(resultShape, 0.5);
                m_viewer->update();
            }
        }
        catch (...) {}
    }

    void MainWindow::OnShowGrid() {
        // Toggle grid visibility
        static bool gridVisible = false;
        gridVisible = !gridVisible;
        m_viewer->ShowGrid(gridVisible);
    }

    void MainWindow::OnShowAxes() {
        // Toggle axes visibility
        static bool axesVisible = true;
        axesVisible = !axesVisible;
        m_viewer->ShowAxes(axesVisible);
    }

    void MainWindow::SetTheme(const QString& theme) {
        m_themeManager->SetTheme(theme);
    }

    // Boolean operations
    void MainWindow::OnBooleanUnion() {
        // Clean up any existing dialog
        if (m_currentBooleanDialog) {
            m_currentBooleanDialog->deleteLater();
            m_currentBooleanDialog = nullptr;
        }

        // Create and show dialog
        m_currentBooleanDialog = new BooleanOperationDialog(BooleanOperationType::Union, this);

        // Connect dialog signals
        connect(m_currentBooleanDialog, &BooleanOperationDialog::selectionModeChanged,
            this, &MainWindow::OnSelectionModeChanged);
        connect(m_currentBooleanDialog, &BooleanOperationDialog::operationRequested,
            this, &MainWindow::OnBooleanOperationRequested);

        m_currentBooleanDialog->show();
        m_currentBooleanDialog->raise();
        m_currentBooleanDialog->activateWindow();
    }

    void MainWindow::OnBooleanIntersection() {
        // Clean up any existing dialog
        if (m_currentBooleanDialog) {
            m_currentBooleanDialog->deleteLater();
            m_currentBooleanDialog = nullptr;
        }

        // Create and show dialog
        m_currentBooleanDialog = new BooleanOperationDialog(BooleanOperationType::Intersection, this);

        // Connect dialog signals
        connect(m_currentBooleanDialog, &BooleanOperationDialog::selectionModeChanged,
            this, &MainWindow::OnSelectionModeChanged);
        connect(m_currentBooleanDialog, &BooleanOperationDialog::operationRequested,
            this, &MainWindow::OnBooleanOperationRequested);

        m_currentBooleanDialog->show();
        m_currentBooleanDialog->raise();
        m_currentBooleanDialog->activateWindow();
    }

    void MainWindow::OnBooleanDifference() {
        // Clean up any existing dialog
        if (m_currentBooleanDialog) {
            m_currentBooleanDialog->deleteLater();
            m_currentBooleanDialog = nullptr;
        }

        // Create and show dialog
        m_currentBooleanDialog = new BooleanOperationDialog(BooleanOperationType::Difference, this);

        // Connect dialog signals
        connect(m_currentBooleanDialog, &BooleanOperationDialog::selectionModeChanged,
            this, &MainWindow::OnSelectionModeChanged);
        connect(m_currentBooleanDialog, &BooleanOperationDialog::operationRequested,
            this, &MainWindow::OnBooleanOperationRequested);

        m_currentBooleanDialog->show();
        m_currentBooleanDialog->raise();
        m_currentBooleanDialog->activateWindow();
    }

    // Modify operations
    void MainWindow::OnFillet() {
        // Clean up any existing dialog
        if (m_currentFilletChamferDialog) {
            m_currentFilletChamferDialog->deleteLater();
            m_currentFilletChamferDialog = nullptr;
        }

        // Create and show dialog
        m_currentFilletChamferDialog = new FilletChamferDialog(FilletChamferType::Fillet, m_viewer, this);

        // Connect dialog signals
        connect(m_currentFilletChamferDialog, &FilletChamferDialog::selectionModeChanged,
            this, &MainWindow::OnSelectionModeChanged);
        connect(m_currentFilletChamferDialog, &FilletChamferDialog::operationRequested,
            this, &MainWindow::OnFilletChamferOperationRequested);

        m_currentFilletChamferDialog->show();
        m_currentFilletChamferDialog->raise();
        m_currentFilletChamferDialog->activateWindow();
    }

    void MainWindow::OnChamfer() {
        // Clean up any existing dialog
        if (m_currentFilletChamferDialog) {
            m_currentFilletChamferDialog->deleteLater();
            m_currentFilletChamferDialog = nullptr;
        }

        // Create and show dialog
        m_currentFilletChamferDialog = new FilletChamferDialog(FilletChamferType::Chamfer, m_viewer, this);

        // Connect dialog signals
        connect(m_currentFilletChamferDialog, &FilletChamferDialog::selectionModeChanged,
            this, &MainWindow::OnSelectionModeChanged);
        connect(m_currentFilletChamferDialog, &FilletChamferDialog::operationRequested,
            this, &MainWindow::OnFilletChamferOperationRequested);

        connect(m_currentFilletChamferDialog, &FilletChamferDialog::highlightFaceRequested,
            m_viewer, &QtOccView::ShowOpFace);
        connect(m_currentFilletChamferDialog, &FilletChamferDialog::clearHighlightRequested,
            m_viewer, &QtOccView::ClearOpFace);

        m_currentFilletChamferDialog->show();
        m_currentFilletChamferDialog->raise();
        m_currentFilletChamferDialog->activateWindow();
    }

    // Selection mode combo box
    void MainWindow::OnSelectionModeComboChanged(int index) {
        if (!m_selectionModeCombo) return;

        // Get the selection mode from combo box data
        cad_core::SelectionMode mode = static_cast<cad_core::SelectionMode>(
            m_selectionModeCombo->itemData(index).toInt());

        // Convert to OpenCASCADE selection mode integers
        int occSelectionMode = 0; // Default to shape
        switch (mode) {
        case cad_core::SelectionMode::Shape:
            occSelectionMode = 0;
            break;
        case cad_core::SelectionMode::Vertex:
            occSelectionMode = 1;
            break;
        case cad_core::SelectionMode::Edge:
            occSelectionMode = 2;
            break;
        case cad_core::SelectionMode::Face:
            occSelectionMode = 4;
            break;
        }

        // Set the selection mode in viewer
        m_viewer->SetSelectionMode(occSelectionMode);

        // Update status bar
        QString modeText = m_selectionModeCombo->itemText(index);
        statusBar()->showMessage("Selection Mode: " + modeText.replace("Select ", ""));
    }

    // Tab management
    void MainWindow::CloseDocumentTab(int index) {
        if (m_tabWidget->count() <= 1) {
            return; // Keep at least one tab
        }

        QtOccView* viewer = qobject_cast<QtOccView*>(m_tabWidget->widget(index));
        if (viewer) {
            // Check for unsaved changes in this tab
            // For now, just close without checking
            m_tabWidget->removeTab(index);
            viewer->deleteLater();
        }
    }

    void MainWindow::OnTabChanged(int index) {
        if (index >= 0 && index < m_tabWidget->count()) {
            m_viewer = qobject_cast<QtOccView*>(m_tabWidget->widget(index));
            UpdateCurrentDocument();
        }
    }

    void MainWindow::NewDocumentTab() {    

    }

    QtOccView* MainWindow::GetCurrentViewer() const {
        if (m_tabWidget && m_tabWidget->currentIndex() >= 0) {
            return qobject_cast<QtOccView*>(m_tabWidget->currentWidget());
        }
        return nullptr;
    }

    void MainWindow::UpdateCurrentDocument() {
        m_viewer = GetCurrentViewer();
        UpdateActions();
        UpdateWindowTitle();
    }




    void MainWindow::CreateSelectionModeCombo() {
        // Create the combo box
        m_selectionModeCombo = new QComboBox(this);
        m_selectionModeCombo->addItem("Select Shape", static_cast<int>(cad_core::SelectionMode::Shape));
        m_selectionModeCombo->addItem("Select Face", static_cast<int>(cad_core::SelectionMode::Face));
        m_selectionModeCombo->addItem("Select Edge", static_cast<int>(cad_core::SelectionMode::Edge));
        m_selectionModeCombo->addItem("Select Vertex", static_cast<int>(cad_core::SelectionMode::Vertex));

        // Set default selection
        m_selectionModeCombo->setCurrentIndex(0); // Shape mode by default

        // Connect signal
        connect(m_selectionModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::OnSelectionModeComboChanged);
    }

    void MainWindow::CreateConsole() {
        // Create console output text edit
        m_console = new QTextEdit(this);
        m_console->setObjectName("console");
        m_console->setMaximumHeight(200);
        m_console->setMinimumHeight(100);
        m_console->setReadOnly(true);
        m_console->setStyleSheet(
            "QTextEdit {"
            "   background-color: #1e1e1e;"
            "   color: #ffffff;"
            "   font-family: 'Consolas', 'Monaco', monospace;"
            "   font-size: 9pt;"
            "   border: 1px solid #3c3c3c;"
            "}"
        );

        // Install custom message handler to redirect qDebug to console
        qInstallMessageHandler([](QtMsgType type, const QMessageLogContext& context, const QString& msg) {
            // Get the main window instance to access console
            foreach(QWidget * widget, QApplication::topLevelWidgets()) {
                MainWindow* mainWindow = qobject_cast<MainWindow*>(widget);
                if (mainWindow && mainWindow->m_console) {
                    QString formattedMsg;
                    switch (type) {
                    case QtDebugMsg:
                        formattedMsg = QString("[DEBUG] %1").arg(msg);
                        break;
                    case QtWarningMsg:
                        formattedMsg = QString("[WARNING] %1").arg(msg);
                        break;
                    case QtCriticalMsg:
                        formattedMsg = QString("[CRITICAL] %1").arg(msg);
                        break;
                    case QtFatalMsg:
                        formattedMsg = QString("[FATAL] %1").arg(msg);
                        break;
                    case QtInfoMsg:
                        formattedMsg = QString("[INFO] %1").arg(msg);
                        break;
                    }
                    mainWindow->m_console->append(formattedMsg);
                    break;
                }
            }
            });

        m_console->append("[SYSTEM] Console initialized");
    }

    // Dialog interaction slots
    void MainWindow::OnSelectionModeChanged(bool enabled, const QString& prompt) {
        if (enabled) {
            // Enable 3D selection mode
            statusBar()->showMessage(prompt);

            // Determine selection mode based on active dialog
            if (m_currentFilletChamferDialog) {
                // For fillet/chamfer operations, switch to edge selection mode
                m_viewer->SetSelectionMode(2); // Edge mode (OpenCASCADE mode 2)
                m_viewer->ClearEdgeSelection(); // Clear previous edge selections

                // Update combo box selection
                if (m_selectionModeCombo) {
                    m_selectionModeCombo->setCurrentIndex(2); // Edge mode
                }
            }
            else if (m_currentBooleanDialog) {
                // For boolean operations, use shape selection mode
                m_viewer->SetSelectionMode(cad_core::SelectionMode::Shape);

                // Update combo box selection
                if (m_selectionModeCombo) {
                    m_selectionModeCombo->setCurrentIndex(0); // Shape mode
                }
            }
            else {
                // Default to shape selection
                m_viewer->SetSelectionMode(cad_core::SelectionMode::Shape);

                // Update combo box selection
                if (m_selectionModeCombo) {
                    m_selectionModeCombo->setCurrentIndex(0); // Shape mode
                }
            }
        }
        else {
            // Disable selection mode
            statusBar()->showMessage("Ready");
            // Return to default shape selection mode
            m_viewer->SetSelectionMode(cad_core::SelectionMode::Shape);
        }
    }

    void MainWindow::OnObjectSelected(const cad_core::ShapePtr& shape) {
        if (!shape) return;

        // If the extrude dialog is open, pass the current selection to it

        // At this stage, accept normal selectable objects first; sketch-profile priority can be added later

        if (m_currentExtrudeDialog) {
            m_currentExtrudeDialog->SetSelectedShape(shape);
        }
        if (m_currentLoftDialog) {
            m_currentLoftDialog->SetSelectedShape(shape);
        }
        if (m_currentRevolveDialog) {
            m_currentRevolveDialog->SetSelectedShape(shape);
        }
        if (m_currentBooleanDialog) {
            m_currentBooleanDialog->onObjectSelected(shape);
        }
        if (m_currentFilletChamferDialog) {
            m_currentFilletChamferDialog->onEdgeSelected(shape);
        }
        if (m_currentTransformDialog) {
            m_currentTransformDialog->onObjectSelected(shape);
        }
    }

    void MainWindow::OnBooleanOperationRequested(BooleanOperationType type,
        const std::vector<cad_core::ShapePtr>& targets,
        const std::vector<cad_core::ShapePtr>& tools) {
        // 1. Basic validation (keep the original logic)

        if (type == BooleanOperationType::Union) {
            if (targets.empty() && tools.empty()) {
                QMessageBox::warning(this, "Boolean Union", "Please select objects to merge.");
                return;
            }
        }
        else {
            if (targets.empty() || tools.empty()) {
                QMessageBox::warning(this, "Boolean Operation", "Please select both target and tool objects.");
                return;
            }
        }

        // 2. Prepare the feature name and transaction

        QString operationName;
        cad_feature::BooleanType featureType;
        switch (type) {
        case BooleanOperationType::Union:
            operationName = "Boolean Union";
            featureType = cad_feature::BooleanType::Union;
            break;
        case BooleanOperationType::Intersection:
            operationName = "Boolean Intersection";
            featureType = cad_feature::BooleanType::Intersection;
            break;
        case BooleanOperationType::Difference:
            operationName = "Boolean Difference";
            featureType = cad_feature::BooleanType::Difference;
            break;
        }

        m_ocafManager->StartTransaction(operationName.toStdString());

        try {
            // 3. Instantiate the boolean feature

            std::string featureName = operationName.toStdString() + "_" + std::to_string(m_featureManager->GetFeatureCount() + 1);
            auto booleanFeature = std::make_shared<cad_feature::BooleanFeature>(featureName);

            // 4. Set the inputs

            booleanFeature->SetOperationType(featureType);
            booleanFeature->SetTargets(targets);
            booleanFeature->SetTools(tools);

            // 5. Let the feature generate the 3D shape

            cad_core::ShapePtr result = booleanFeature->CreateShape();

            if (result) {
                //Bind the generated shape to the feature

                booleanFeature->SetResultShape(result);

                // Register the new shape in OCAF

                if (m_ocafManager->AddShape(result, featureName)) {

                    // Register the feature in the feature manager and the Features node in the document tree

                    m_featureManager->AddFeature(booleanFeature);
                    m_documentTree->AddFeature(booleanFeature);

                    // Display the generated body in the 3D view and document tree

                    m_viewer->DisplayShape(result);
                    m_documentTree->AddShape(result);

                    // Hide the absorbed parent bodies

                    auto hideAbsorbedShapes = [&](const std::vector<cad_core::ShapePtr>& shapes) {
                        for (const auto& shape : shapes) {
                            m_ocafManager->RemoveShape(shape);     // 真正从 OCAF 文档移除
                            m_viewer->RemoveShape(shape);          // 从 3D viewer 移除（不是只 hide）
                            m_documentTree->RemoveShape(shape);    // 从 tree 移除

                        }
                        };

                    hideAbsorbedShapes(targets);
                    hideAbsorbedShapes(tools);

                    m_ocafManager->CommitTransaction();
                    m_commandManager->ExecuteCommand(
                        std::make_shared<cad_core::OCAFTransactionCommand>(m_ocafManager.get(), operationName.toStdString()));
                    SetDocumentModified(true);
                    UpdateActions();
                    statusBar()->showMessage(operationName + " completed successfully");
                }
                else {
                    m_ocafManager->AbortTransaction();
                    QMessageBox::warning(this, "Error", "Failed to add result to document.");
                }
            }
            else {
                m_ocafManager->AbortTransaction();
                QMessageBox::warning(this, "Error", operationName + " operation failed.");
            }
        }
        catch (const std::exception& e) {
            m_ocafManager->AbortTransaction();
            QMessageBox::warning(this, "Error", QString("Boolean operation failed: %1").arg(e.what()));
        }

        // Clean up the dialog

        if (m_currentBooleanDialog) {
            m_currentBooleanDialog->deleteLater();
            m_currentBooleanDialog = nullptr;
        }
    }

    void MainWindow::OnFilletChamferOperationRequested(FilletChamferType type,
        const std::vector<cad_core::ShapePtr>& edges,
        double radius, double distance1, double distance2) {
        if (edges.empty()) {
            QMessageBox::warning(this, "Fillet/Chamfer", "Please select edges for operation.");
            return;
        }

        // Get selected edges grouped by their parent shapes
        auto edgesByShape = m_viewer->GetSelectedEdgesByShape();
        if (edgesByShape.empty()) {
            QMessageBox::warning(this, "Fillet/Chamfer", "No edges selected in 3D view. Please select edges first.");
            return;
        }

        qDebug() << "Fillet/Chamfer operation requested with edges from" << edgesByShape.size() << "shape(s)";

        // Start OCAF transaction
        QString operationName = (type == FilletChamferType::Fillet) ? "Fillet" : "Chamfer";
        m_ocafManager->StartTransaction(operationName.toStdString());

        try {
            bool anySuccess = false;

            for (const auto& shapeEdgePair : edgesByShape) {
                cad_core::ShapePtr baseShape = shapeEdgePair.first;
                const std::vector<TopoDS_Edge>& faceEdges = shapeEdgePair.second; // Avoid variable shadowing


                if (!baseShape || faceEdges.empty()) continue;

                // 1. Create the feature object

                std::string featureName = operationName.toStdString() + "_" + std::to_string(m_featureManager->GetFeatureCount() + 1);
                auto fcFeature = std::make_shared<cad_feature::FilletChamferFeature>(featureName);

                // 2. Assign parameters

                fcFeature->SetOperationType(type == FilletChamferType::Fillet ? cad_feature::FCType::Fillet : cad_feature::FCType::Chamfer);
                fcFeature->SetBaseShape(baseShape);
                fcFeature->SetEdges(faceEdges);
                fcFeature->SetRadius(radius);
                fcFeature->SetDistance1(distance1);
                fcFeature->SetDistance2(distance2);

                // 3. Run the algorithm and generate the result

                auto resultShape = fcFeature->CreateShape();

                if (resultShape) {
                    // Bind the result

                    fcFeature->SetResultShape(resultShape);

                    if (m_ocafManager->AddShape(resultShape, featureName)) {
                        // Add the feature to the tree

                        m_featureManager->AddFeature(fcFeature);
                        m_documentTree->AddFeature(fcFeature);

                        // Add the new shape to the tree and display it

                        m_viewer->DisplayShape(resultShape);
                        m_documentTree->AddShape(resultShape);

                        // Hide and remove the original entity 

                        m_viewer->SetShapeVisibility(baseShape, false);
                        m_documentTree->RemoveShape(baseShape);

                        anySuccess = true;
                    }
                }
            }

            if (anySuccess) {
                m_ocafManager->CommitTransaction();
                m_commandManager->ExecuteCommand(
                    std::make_shared<cad_core::OCAFTransactionCommand>(m_ocafManager.get(), operationName.toStdString()));
                SetDocumentModified(true);
                UpdateActions();
                statusBar()->showMessage(operationName + " completed successfully");
            }
            else {
                m_ocafManager->AbortTransaction();
                QMessageBox::warning(this, "Error", operationName + " operation failed.");
            }
        }
        catch (const std::exception& e) {
            m_ocafManager->AbortTransaction();
            QMessageBox::warning(this, "Error", QString("%1 operation failed: %2").arg(operationName).arg(e.what()));
        }

        // Clear edge selection after operation
        m_viewer->ClearEdgeSelection();

        // Clean up dialog
        if (m_currentFilletChamferDialog) {
            m_currentFilletChamferDialog->deleteLater();
            m_currentFilletChamferDialog = nullptr;
        }
    }

    // =============================================================================
    // Transform Operations Implementation
    // =============================================================================

    void MainWindow::OnTransformObjects() {
        if (m_currentTransformDialog) {
            m_currentTransformDialog->show();
            m_currentTransformDialog->raise();
            m_currentTransformDialog->activateWindow();
            return;
        }

        m_currentTransformDialog = new TransformOperationDialog(this);
        m_currentTransformDialog->SetOCAFManager(m_ocafManager.get());

        // Connect dialog signals
        connect(m_currentTransformDialog, &TransformOperationDialog::selectionModeChanged,
            this, &MainWindow::OnSelectionModeChanged);
        connect(m_currentTransformDialog, &TransformOperationDialog::transformRequested,
            this, &MainWindow::OnTransformOperationRequested);
        connect(m_currentTransformDialog, &TransformOperationDialog::previewRequested,
            this, &MainWindow::OnTransformPreviewRequested);
        connect(m_currentTransformDialog, &TransformOperationDialog::resetRequested,
            this, &MainWindow::OnTransformResetRequested);

        // Show dialog
        m_currentTransformDialog->show();
    }

    void MainWindow::OnTransformOperationRequested(std::shared_ptr<cad_core::TransformCommand> command) {
        if (!command) {
            return;
        }

        try {
            if (m_previewActive) {
                OnTransformResetRequested();
            }

       
            if (m_commandManager->ExecuteCommand(command)) {
                RefreshUIFromOCAF();
                SetDocumentModified(true);
                UpdateActions();
                statusBar()->showMessage(
                    QString("Transformation operation completed: %1").arg(command->GetName()), 2000);
            }
            else {
                QMessageBox::warning(this, "Error", "Transformation operation execution failed");
            }
        }
        catch (const std::exception& e) {
            QMessageBox::warning(this, "Error",
                QString("Transformation operation failed: %1").arg(e.what()));
        }

        if (m_currentTransformDialog) {
            m_currentTransformDialog->deleteLater();
            m_currentTransformDialog = nullptr;
        }
    }

    void MainWindow::OnTransformPreviewRequested(std::shared_ptr<cad_core::TransformCommand> command) {
        if (!command) {
            return;
        }

        try {
            // Clear any existing preview
            if (m_previewActive) {
                OnTransformResetRequested();
            }

            // Get preview shapes from command
            auto previewShapes = command->GetTransformedShapes();

            if (!previewShapes.empty()) {
                // Store preview shapes
                m_previewShapes = previewShapes;
                m_previewActive = true;

                // Display preview shapes with a different color/style
                for (const auto& shape : m_previewShapes) {
                    if (shape && shape->IsValid()) {
                        // TODO: Set preview material/color (semi-transparent or different color)
                        m_viewer->DisplayShape(shape);
                    }
                }

                // Update display
                m_viewer->update();
            }
        }
        catch (const std::exception& e) {
            QMessageBox::warning(this, "Error", QString("Preview generation failed: %1").arg(e.what()));
        }
    }

    void MainWindow::OnTransformResetRequested() {
        if (!m_previewActive) {
            return;
        }

        // Remove preview shapes from display
        for (const auto& shape : m_previewShapes) {
            if (shape) {
                m_viewer->RemoveShape(shape);
            }
        }

        // Clear preview data
        m_previewShapes.clear();
        m_previewActive = false;

        // Update display
        m_viewer->update();
    }

    void MainWindow::OnFeatureParameterChanged(const cad_feature::FeaturePtr& feature) {
        if (!feature) return;

        // Start a transaction to record the modification

        m_ocafManager->StartTransaction("Modify Feature Parameter");

        try {
            // 1. Retrieve the old 3D shape owned by this feature

            auto oldShape = feature->GetResultShape();
            if (oldShape) {
                // Remove it completely from OCAF, the viewer, and the document tree

                m_ocafManager->RemoveShape(oldShape);
                m_viewer->RemoveShape(oldShape);
                m_documentTree->RemoveShape(oldShape);
            }

            // 2. Regenerate the feature (CreateShape reads the updated Width and Height)

            auto newShape = feature->CreateShape();

            if (newShape) {
                // 3. Rebind the new shape to the feature

                feature->SetResultShape(newShape);

                // 4. Register the new shape in all three subsystems again

                if (m_ocafManager->AddShape(newShape, feature->GetName())) {
                    m_viewer->DisplayShape(newShape);
                    m_documentTree->AddShape(newShape);

                    m_ocafManager->CommitTransaction();
                    m_commandManager->ExecuteCommand(
                        std::make_shared<cad_core::OCAFTransactionCommand>(m_ocafManager.get(), "Modify Feature Parameter"));
                    SetDocumentModified(true);

                    // Refresh the screen so the resize takes effect immediately

                    m_viewer->update();
                }
                else {
                    throw std::runtime_error("Failed to add modified shape to document.");
                }
            }
            else {
                throw std::runtime_error("Failed to generate new shape with current parameters.");
            }
        }
        catch (const std::exception& e) {
            m_ocafManager->AbortTransaction();
            QMessageBox::warning(this, "Parameter Update Error", e.what());

        }
    }


    // =============================================================================
    // Sketch Mode Implementation
    // =============================================================================

    void MainWindow::OnEnterSketchMode() {
        if (!m_viewer) {
            qDebug() << "Error: No viewer available";
            return;
        }

        try {
            // Check whether sketch mode is already active

            if (m_viewer->IsInSketchMode()) {
                qDebug() << "Already in sketch mode";
                return;
            }

            // Check whether there is an available object

            auto shapes = m_ocafManager->GetAllShapes();
            if (shapes.empty()) {
                if (m_statusBar) {
                    statusBar()->showMessage("Create a geometric shape (such as a box), and then select a surface to enter the sketch mode.");
                }
                qDebug() << "No shapes available for face selection";
                return;
            }

            // Create and show the face-selection dialog

            FaceSelectionDialog* dialog = new FaceSelectionDialog(m_viewer, this);

            // Connect dialog signals

            connect(dialog, &FaceSelectionDialog::faceSelected, this, [this, dialog](const TopoDS_Face& face) {
                OnFaceSelectedForSketch(face);
                dialog->close();
                dialog->deleteLater();
                });

            connect(dialog, &FaceSelectionDialog::selectionCancelled, this, [this, dialog]() {
                if (m_statusBar) {
                    statusBar()->showMessage("The sketch mode has been cancelled.");
                }
                dialog->close();
                dialog->deleteLater();
                });

            // Show the dialog in non-modal mode

            dialog->show();

            qDebug() << "Face selection dialog shown";
        }
        catch (const std::exception& e) {
            qDebug() << "Error in OnEnterSketchMode:" << e.what();
            if (m_statusBar) {
                statusBar()->showMessage("Failed to enter the sketch mode");
            }
        }
    }

    void MainWindow::OnExitSketchMode() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) {
            return;
        }

        // Capture the current sketch and add it to the tree before exiting

        auto activeSketch = m_viewer->GetActiveSketch();
        if (activeSketch && !activeSketch->GetElements().empty()) {
            m_documentTree->AddSketch(activeSketch);
        }

        // Exit the sketch-mode viewer state

        m_viewer->ExitSketchMode();

        // update UI state

        m_enterSketchAction->setEnabled(true);
        m_exitSketchAction->setEnabled(false);
        m_sketchRectangleAction->setEnabled(false);
        m_sketchPointAction->setEnabled(false);
        m_sketchLineAction->setEnabled(false);
        m_sketchCircleAction->setEnabled(false);
        m_sketchArcAction->setEnabled(false);

        m_viewer->SetSelectionMode(0); // Restore Shape selection mode

        m_waitingForFaceSelection = false;
        statusBar()->showMessage("Sketch mode exited.");
        UpdateActions();
    }

    // Sketch tool slot functions

    void MainWindow::OnSketchRectangleTool() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;

        if (m_sketchRectangleAction->isChecked()) {
            m_viewer->StartRectangleTool();
            statusBar()->showMessage("The rectangle tool is activated - click and drag to create a rectangle");
        }
        else {
            m_viewer->StopSketchTool(); // Click the button again to cancel the tool

        }
    }

    void MainWindow::OnSketchPointTool() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;

        if (m_sketchPointAction->isChecked()) {
            m_viewer->StartPointTool();
            statusBar()->showMessage("The point tool is activated - click to create a point");
        }
        else {
            m_viewer->StopSketchTool(); // Click the button again to cancel the tool

        }
    }

    void MainWindow::OnSketchLineTool() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;

        if (m_sketchLineAction->isChecked()) {
            m_viewer->StartLineTool();
            statusBar()->showMessage("The line tool is activated - click and move to create a line");
        }
        else {
            m_viewer->StopSketchTool(); // Click the button again to cancel the tool

        }
    }

    void MainWindow::OnSketchCircleTool() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;

        if (m_sketchCircleAction->isChecked()) {
            m_viewer->StartCircleTool();
            statusBar()->showMessage("The circle tool is activated - click for center, drag for radius");
        }
        else {
            m_viewer->StopSketchTool(); // Click the button again to cancel the tool

        }
    }

    void MainWindow::OnSketchArcTool() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;

        if (m_sketchArcAction->isChecked()) {
            m_viewer->StartArcTool();
            statusBar()->showMessage("The arc tool is activated - click center, start, and end points");
        }
        else {
            m_viewer->StopSketchTool(); // Click the button again to cancel the tool

        }
    }

    void MainWindow::OnSketchCurveTool() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;

        if (m_sketchCurveAction->isChecked()) {
            m_viewer->StartCurveTool(); // Call the interface exposed by QtOccView

            statusBar()->showMessage("The curve tool is activated - click points to generate a curve, right-click to finish");
        }
        else {
            m_viewer->StopSketchTool();
        }
    }

    void MainWindow::OnFaceSelected(const TopoDS_Face& face) {
        // Base-face selection logic before entering sketch mode

        if (m_waitingForFaceSelection) {
            m_waitingForFaceSelection = false;
            m_viewer->SetSelectionMode(0);
            if (m_selectionModeCombo) {
                m_selectionModeCombo->setCurrentIndex(0);
            }

            m_viewer->EnterSketchMode(face);

            UpdateActions();
            return;
        }

        if (m_currentLoftDialog) {
            auto faceShape = std::make_shared<cad_core::Shape>(face);
            m_currentLoftDialog->SetSelectedShape(faceShape);
        }

    }

    void MainWindow::OnFaceSelectedForSketch(const TopoDS_Face& face) {
        try {
            // Check whether the face is valid

            if (face.IsNull()) {
                qDebug() << "Error: Selected face is null";
                if (m_statusBar) {
                    statusBar()->showMessage("The selected option is invalid.");
                }
                return;
            }

            // Enter sketch mode directly

            if (m_viewer) {
                m_viewer->EnterSketchMode(face);
                if (m_statusBar) {
                    statusBar()->showMessage("Entering sketch mode...");
                }
            }
            else {
                qDebug() << "Error: No viewer available for sketch mode";
                if (m_statusBar) {
                    statusBar()->showMessage("No viewer available");
                }
            }

            qDebug() << "Face selected from dialog, entering sketch mode";
        }
        catch (const std::exception& e) {
            qDebug() << "Error in OnFaceSelectedForSketch:" << e.what();
            if (m_statusBar) {
                statusBar()->showMessage(QString("Error in OnFaceSelectedForSketch: %1").arg(e.what()));
            }
        }
    }

    void MainWindow::OnSketchModeEntered() {
        // Update UI state when sketch mode is entered
        m_enterSketchAction->setEnabled(false);
        m_exitSketchAction->setEnabled(true);
        m_sketchRectangleAction->setEnabled(true);
        m_sketchPointAction->setEnabled(true);
        m_sketchLineAction->setEnabled(true);
        m_sketchCircleAction->setEnabled(true);
        m_sketchArcAction->setEnabled(true);
        m_sketchCurveAction->setEnabled(true);

        m_constraintHorizontalAction->setEnabled(true);
        m_constraintVerticalAction->setEnabled(true);
        m_constraintCoincidentAction->setEnabled(true);
        m_constraintDistanceAction->setEnabled(true);
        m_constraintParallelAction->setEnabled(true);
        m_constraintPerpendicularAction->setEnabled(true);
        m_constraintAngleAction->setEnabled(true);
        m_constraintEqualLengthAction->setEnabled(true);
        m_constraintFixedAction->setEnabled(true);
        m_constraintRadiusAction->setEnabled(true);

        // Reset selection mode
        //m_viewer->SetSelectionMode(0);  // Shape selection mode

        statusBar()->showMessage(QString("Entered the sketch mode - Select the drawing tool to start drawing"));

        UpdateActions();

        qDebug() << "Sketch mode entered, UI updated";
    }

    void MainWindow::OnEditSketchRequested(const std::shared_ptr<cad_sketch::Sketch>& sketch) {
        if (!sketch || !m_viewer) return;

        m_viewer->ClearSelection();

        m_viewer->setFocus();

        m_viewer->EditSketch(sketch);

        UpdateActions();
    }

    void MainWindow::OnSketchModeExited() {
        // Update UI state when sketch mode is exited
        m_enterSketchAction->setEnabled(true);
        m_exitSketchAction->setEnabled(false);
        m_sketchRectangleAction->setEnabled(false);
        m_sketchPointAction->setEnabled(false);
        m_sketchLineAction->setEnabled(false);
        m_sketchCircleAction->setEnabled(false);
        m_sketchArcAction->setEnabled(false);
        m_sketchCurveAction->setEnabled(false);

        m_constraintHorizontalAction->setEnabled(false);
        m_constraintVerticalAction->setEnabled(false);
        m_constraintCoincidentAction->setEnabled(false);
        m_constraintDistanceAction->setEnabled(false);
        m_constraintParallelAction->setEnabled(false);
        m_constraintPerpendicularAction->setEnabled(false);
        m_constraintAngleAction->setEnabled(false);
        m_constraintEqualLengthAction->setEnabled(false);
        m_constraintFixedAction->setEnabled(false);
        m_constraintRadiusAction->setEnabled(false);

        m_viewer->SetSelectionMode(0);

        // Reset any waiting states
        m_waitingForFaceSelection = false;

        statusBar()->showMessage("Sketch mode exited");

        UpdateActions();

        qDebug() << "Sketch mode exited, UI updated";
    }

    // Synchronize the UI state with the active underlying tool

    void MainWindow::OnSketchToolChanged(const QString& toolName) {
        // Temporarily block signals to avoid recursive triggered events from setChecked()

        m_sketchRectangleAction->blockSignals(true);
        m_sketchLineAction->blockSignals(true);
        m_sketchCircleAction->blockSignals(true);
        m_sketchArcAction->blockSignals(true);
        m_sketchPointAction->blockSignals(true);
        m_sketchCurveAction->blockSignals(true);

        if (toolName == "None") {
            // State 1: no tool is active (for example, after Esc or manual cancellation)

            // Re-enable all buttons and restore their active appearance

            m_sketchRectangleAction->setEnabled(true);
            m_sketchLineAction->setEnabled(true);
            m_sketchCircleAction->setEnabled(true);
            m_sketchArcAction->setEnabled(true);
            m_sketchPointAction->setEnabled(true);
            m_sketchCurveAction->setEnabled(true);

            // Set all buttons to the unchecked state

            m_sketchRectangleAction->setChecked(false);
            m_sketchLineAction->setChecked(false);
            m_sketchCircleAction->setChecked(false);
            m_sketchArcAction->setChecked(false);
            m_sketchPointAction->setChecked(false);
            m_sketchCurveAction->setChecked(false);

            statusBar()->showMessage("Ready - Select sketch elements to modify or delete.");
        }
        else {
            // State 2: a specific tool is active

            // Only keep the active tool enabled so the user can click it again to cancel; disable the others

            m_sketchRectangleAction->setEnabled(toolName == "Rectangle");
            m_sketchLineAction->setEnabled(toolName == "Line");
            m_sketchCircleAction->setEnabled(toolName == "Circle");
            m_sketchArcAction->setEnabled(toolName == "Arc");
            m_sketchPointAction->setEnabled(toolName == "Point");
            m_sketchCurveAction->setEnabled(toolName == "Curve");

            // Only keep the active button checked

            m_sketchRectangleAction->setChecked(toolName == "Rectangle");
            m_sketchLineAction->setChecked(toolName == "Line");
            m_sketchCircleAction->setChecked(toolName == "Circle");
            m_sketchArcAction->setChecked(toolName == "Arc");
            m_sketchPointAction->setChecked(toolName == "Point");
            m_sketchCurveAction->setChecked(toolName == "Curve");
        }

        // Restore normal signal delivery

        m_sketchRectangleAction->blockSignals(false);
        m_sketchLineAction->blockSignals(false);
        m_sketchCircleAction->blockSignals(false);
        m_sketchArcAction->blockSignals(false);
        m_sketchPointAction->blockSignals(false);
        m_sketchCurveAction->blockSignals(false);
    }

    // =========================================================================
    // Constraint slot implementations

    // =========================================================================

    // Collect selected sketch elements and classify them by type


    struct SketchSelection {
        std::vector<cad_sketch::SketchPointPtr> points;
        std::vector<cad_sketch::SketchLinePtr> lines;
        std::vector<cad_sketch::SketchCirclePtr> circles;
        std::vector<cad_sketch::SketchArcPtr> arcs;
    };

    static SketchSelection ClassifySelectedElements(cad_ui::QtOccView* viewer) {
        SketchSelection sel;
        auto elements = viewer->GetSelectedSketchElements();
        for (const auto& elem : elements) {
            if (auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(elem)) {
                sel.lines.push_back(line);
            }
            else if (auto circle = std::dynamic_pointer_cast<cad_sketch::SketchCircle>(elem)) {
                sel.circles.push_back(circle);
            }
            else if (auto arc = std::dynamic_pointer_cast<cad_sketch::SketchArc>(elem)) {
                sel.arcs.push_back(arc);
            }
            else if (auto point = std::dynamic_pointer_cast<cad_sketch::SketchPoint>(elem)) {
                sel.points.push_back(point);
            }
        }
        // Also collect line endpoints when points are required

        return sel;
    }

    // Common post-processing after applying a constraint: solve, refresh, and clear selection

    void MainWindow::ApplyConstraintAndRefresh(const cad_sketch::ConstraintPtr& constraint) {
        auto sketch = m_viewer->GetActiveSketch();
        if (!sketch || !constraint) return;

        // Stop the current drawing tool first to avoid conflicts

        if (m_viewer->HasActiveSketchTool()) {
            m_viewer->StopSketchTool();
        }

        sketch->AddConstraint(constraint);
        bool ok = sketch->SolveConstraints();

        auto result = sketch->GetConstraintSolver()->GetLastResult();

        // Clear multi-selection and highlights whether the solve succeeds or fails

        m_viewer->SetMultiSelectionMode(false);
        m_viewer->ClearSelection();

        if (ok) {
            // Refresh the view: clear old sketch graphics and redraw

            gp_Ax3 cs = m_viewer->GetSketchCS();
            m_viewer->ClearSketchObjects();
            m_viewer->AddSketchElements(sketch->GetElements(), cs);
            sketch->UpdateProfiles(cs);
            m_viewer->ClearSketchProfiles();
            m_viewer->RenderSketchProfiles(sketch->GetProfiles());

            statusBar()->showMessage(QString("Constraint applied (converged in %1 iterations)")
                .arg(result.iterations));
        }
        else {
            // If solving fails, remove the newly added constraint

            sketch->RemoveConstraint(constraint);
            statusBar()->showMessage(QString("Constraint failed to solve (error=%1)")
                .arg(result.finalError));
            QMessageBox::warning(this, "Constraint Error",
                "The constraint could not be satisfied.\n"
                "It may conflict with existing constraints.");
        }
    }

    // ---------- 1. Horizontal ----------
    void MainWindow::OnConstraintHorizontal() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;
        auto sel = ClassifySelectedElements(m_viewer);

        if (sel.lines.empty()) {
            statusBar()->showMessage("Please select a line first, then click Horizontal");
            return;
        }
        for (auto& line : sel.lines) {
            auto c = std::make_shared<cad_sketch::HorizontalConstraint>(line);
            ApplyConstraintAndRefresh(c);
        }
    }

    // ---------- 2. Vertical ----------
    void MainWindow::OnConstraintVertical() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;
        auto sel = ClassifySelectedElements(m_viewer);

        if (sel.lines.empty()) {
            statusBar()->showMessage("Please select a line first, then click Vertical");
            return;
        }
        for (auto& line : sel.lines) {
            auto c = std::make_shared<cad_sketch::VerticalConstraint>(line);
            ApplyConstraintAndRefresh(c);
        }
    }

    // ---------- 3. Coincident ----------
    void MainWindow::OnConstraintCoincident() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;
        auto sel = ClassifySelectedElements(m_viewer);

        // Collect all available points: standalone points and line endpoints

        std::vector<cad_sketch::SketchPointPtr> allPoints = sel.points;
        for (auto& line : sel.lines) {
            allPoints.push_back(line->GetStartPoint());
            allPoints.push_back(line->GetEndPoint());
        }

        if (allPoints.size() < 2) {
            statusBar()->showMessage("Please select 2 points (or 2 line endpoints) for Coincident");
            return;
        }

        // Make the first point coincide with the second point

        auto c = std::make_shared<cad_sketch::CoincidentConstraint>(allPoints[0], allPoints[1]);
        ApplyConstraintAndRefresh(c);
    }

    // ---------- 4. Distance ----------
    void MainWindow::OnConstraintDistance() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;
        auto sel = ClassifySelectedElements(m_viewer);

        // If one line is selected, constrain its length

        // If two points are selected, constrain the distance between them

        cad_sketch::SketchPointPtr p1, p2;
        double currentDist = 0.0;

        if (!sel.lines.empty()) {
            p1 = sel.lines[0]->GetStartPoint();
            p2 = sel.lines[0]->GetEndPoint();
            currentDist = sel.lines[0]->GetLength();
        }
        else {
            std::vector<cad_sketch::SketchPointPtr> allPoints = sel.points;
            for (auto& line : sel.lines) {
                allPoints.push_back(line->GetStartPoint());
                allPoints.push_back(line->GetEndPoint());
            }
            if (allPoints.size() < 2) {
                statusBar()->showMessage("Please select a line or 2 points for Distance constraint");
                return;
            }
            p1 = allPoints[0];
            p2 = allPoints[1];
            currentDist = p1->GetPoint().Distance(p2->GetPoint());
        }

        // Open a dialog for the target distance

        bool ok;
        double targetDist = QInputDialog::getDouble(this, "Distance Constraint",
            "Enter target distance:", currentDist, 0.001, 99999.0, 3, &ok);
        if (!ok) return;

        auto c = std::make_shared<cad_sketch::DistanceConstraint>(p1, p2, targetDist);
        ApplyConstraintAndRefresh(c);
    }

    // ---------- 5. Parallel ----------
    void MainWindow::OnConstraintParallel() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;
        auto sel = ClassifySelectedElements(m_viewer);

        if (sel.lines.size() < 2) {
            statusBar()->showMessage("Please select 2 lines for Parallel constraint");
            return;
        }
        auto c = std::make_shared<cad_sketch::ParallelConstraint>(sel.lines[0], sel.lines[1]);
        ApplyConstraintAndRefresh(c);
    }

    // ---------- 6. Perpendicular ----------
    void MainWindow::OnConstraintPerpendicular() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;
        auto sel = ClassifySelectedElements(m_viewer);

        if (sel.lines.size() < 2) {
            statusBar()->showMessage("Please select 2 lines for Perpendicular constraint");
            return;
        }
        auto c = std::make_shared<cad_sketch::PerpendicularConstraint>(sel.lines[0], sel.lines[1]);
        ApplyConstraintAndRefresh(c);
    }

    // ---------- 7. Angle ----------
    void MainWindow::OnConstraintAngle() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;
        auto sel = ClassifySelectedElements(m_viewer);

        if (sel.lines.size() < 2) {
            statusBar()->showMessage("Please select 2 lines for Angle constraint");
            return;
        }

        // Calculate the current angle

        double dx1 = sel.lines[0]->GetEndPoint()->GetX() - sel.lines[0]->GetStartPoint()->GetX();
        double dy1 = sel.lines[0]->GetEndPoint()->GetY() - sel.lines[0]->GetStartPoint()->GetY();
        double dx2 = sel.lines[1]->GetEndPoint()->GetX() - sel.lines[1]->GetStartPoint()->GetX();
        double dy2 = sel.lines[1]->GetEndPoint()->GetY() - sel.lines[1]->GetStartPoint()->GetY();
        double currentAngleDeg = std::atan2(dx1 * dy2 - dy1 * dx2, dx1 * dx2 + dy1 * dy2) * 180.0 / M_PI;

        bool ok;
        double targetDeg = QInputDialog::getDouble(this, "Angle Constraint",
            "Enter target angle (degrees):", std::abs(currentAngleDeg), 0.0, 360.0, 2, &ok);
        if (!ok) return;

        double targetRad = targetDeg * M_PI / 180.0;
        auto c = std::make_shared<cad_sketch::AngleConstraint>(sel.lines[0], sel.lines[1], targetRad);
        ApplyConstraintAndRefresh(c);
    }

    // ---------- 8. Equal Length ----------
    void MainWindow::OnConstraintEqualLength() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;
        auto sel = ClassifySelectedElements(m_viewer);

        if (sel.lines.size() < 2) {
            statusBar()->showMessage("Please select 2 lines for Equal Length constraint");
            return;
        }
        auto c = std::make_shared<cad_sketch::EqualLengthConstraint>(sel.lines[0], sel.lines[1]);
        ApplyConstraintAndRefresh(c);
    }

    // ---------- 9. Fixed ----------
    void MainWindow::OnConstraintFixed() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;
        auto sel = ClassifySelectedElements(m_viewer);

        // Collect all points

        std::vector<cad_sketch::SketchPointPtr> allPoints = sel.points;
        for (auto& line : sel.lines) {
            allPoints.push_back(line->GetStartPoint());
            allPoints.push_back(line->GetEndPoint());
        }

        if (allPoints.empty()) {
            statusBar()->showMessage("Please select a point (or line endpoint) to fix");
            return;
        }

        // Fix the first point at its current position

        auto c = std::make_shared<cad_sketch::FixedConstraint>(allPoints[0]);
        ApplyConstraintAndRefresh(c);
    }

    // ---------- 10. Radius ----------
    void MainWindow::OnConstraintRadius() {
        if (!m_viewer || !m_viewer->IsInSketchMode()) return;
        auto sel = ClassifySelectedElements(m_viewer);

        if (sel.circles.empty() && sel.arcs.empty()) {
            statusBar()->showMessage("Please select a circle or arc for Radius constraint");
            return;
        }

        double currentRadius = 0.0;
        if (!sel.circles.empty()) {
            currentRadius = sel.circles[0]->GetRadius();
        }
        else {
            currentRadius = sel.arcs[0]->GetRadius();
        }

        bool ok;
        double targetRadius = QInputDialog::getDouble(this, "Radius Constraint",
            "Enter target radius:", currentRadius, 0.001, 99999.0, 3, &ok);
        if (!ok) return;

        cad_sketch::ConstraintPtr c;
        if (!sel.circles.empty()) {
            c = std::make_shared<cad_sketch::RadiusConstraint>(sel.circles[0], targetRadius);
        }
        else {
            c = std::make_shared<cad_sketch::RadiusConstraint>(sel.arcs[0], targetRadius);
        }
        ApplyConstraintAndRefresh(c);
    }

    void MainWindow::ClearPreview() {
        if (!m_previewActive) return;

		// remove preview shapes from the viewer
        for (const auto& shape : m_previewShapes) {
            if (shape) {
                m_viewer->RemoveShape(shape);
            }
        }

        m_previewShapes.clear();
        m_previewActive = false;
        m_viewer->update();
    }

} // namespace cad_ui

#include "MainWindow.moc"