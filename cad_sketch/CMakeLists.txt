set(TARGET_NAME cad_sketch)

# 头文件
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
    include/cad_sketch/Constraintsolvertest.h
)

# 源文件
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

# 创建静态库
add_library(${TARGET_NAME} STATIC ${HEADERS} ${SOURCES})

# 包含目录
target_include_directories(${TARGET_NAME} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCASCADE_INCLUDE_DIR}
)

# 添加 Eigen 依赖 
find_package(Eigen3 REQUIRED)

target_link_libraries(${TARGET_NAME} 
    cad_core
    ${OpenCASCADE_LIBRARIES}
    Eigen3::Eigen                               # 新增
)

# 链接库
target_link_libraries(${TARGET_NAME} 
    cad_core
    ${OpenCASCADE_LIBRARIES}
)