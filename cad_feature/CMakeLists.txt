set(TARGET_NAME cad_feature)

# Header files
set(HEADERS
    include/cad_feature/Feature.h
    include/cad_feature/BooleanFeature.h
    include/cad_feature/RectangularFaceFeature.h
    include/cad_feature/FilletChamferFeature.h
    include/cad_feature/ExtrudeFeature.h
    include/cad_feature/RevolveFeature.h
    include/cad_feature/SweepFeature.h
    include/cad_feature/LoftFeature.h
    include/cad_feature/FeatureManager.h
    include/cad_feature/ParameterPanel.h
    include/cad_feature/LivePreview.h
)

# Source files
set(SOURCES
    src/Feature.cpp
    src/BooleanFeature.cpp
    src/RectangularFaceFeature.cpp
    src/FilletChamferFeature.cpp
    src/ExtrudeFeature.cpp
    src/RevolveFeature.cpp
    src/SweepFeature.cpp
    src/LoftFeature.cpp
    src/FeatureManager.cpp
    src/ParameterPanel.cpp
    src/LivePreview.cpp
)

# Create static library
add_library(${TARGET_NAME} STATIC ${HEADERS} ${SOURCES})

# Include directories
target_include_directories(${TARGET_NAME} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCASCADE_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${TARGET_NAME} 
    cad_core
    cad_sketch
    ${OpenCASCADE_LIBRARIES}
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
)