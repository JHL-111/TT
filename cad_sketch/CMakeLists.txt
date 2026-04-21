set(TARGET_NAME cad_sketch)

# Header files
set(HEADERS
    include/cad_sketch/SketchPoint.h
    include/cad_sketch/SketchLine.h
    include/cad_sketch/SketchCircle.h
    include/cad_sketch/SketchArc.h
    include/cad_sketch/SketchCurve.h
    include/cad_sketch/SketchElement.h
    include/cad_sketch/Constraint.h
    include/cad_sketch/ConstraintSolver.h
    include/cad_sketch/Concreteconstraints.h
    include/cad_sketch/Sketch.h
    include/cad_sketch/SketchProfile.h
    include/cad_sketch/SnappingManager.h
)

# Source files
set(SOURCES
    src/SketchPoint.cpp
    src/SketchLine.cpp
    src/SketchCircle.cpp
    src/SketchArc.cpp
    src/SketchCurve.cpp
    src/SketchElement.cpp
    src/Constraint.cpp
    src/ConstraintSolver.cpp
    src/Concreteconstraints.cpp
    src/Sketch.cpp
    src/SketchProfile.cpp
    src/SnappingManager.cpp
)

# Create static library
add_library(${TARGET_NAME} STATIC ${HEADERS} ${SOURCES})

# Include directories
target_include_directories(${TARGET_NAME} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCASCADE_INCLUDE_DIR}
)

# Add Eigen dependency
find_package(Eigen3 REQUIRED)

target_link_libraries(${TARGET_NAME} 
    cad_core
    ${OpenCASCADE_LIBRARIES}
    Eigen3::Eigen                          
)

# Link libraries
target_link_libraries(${TARGET_NAME} 
    cad_core
    ${OpenCASCADE_LIBRARIES}
)