set(TARGET_NAME cad_app)

# 源文件
set(SOURCES
    src/main.cpp
)

# 创建可执行文件
add_executable(${TARGET_NAME} ${SOURCES})

# 包含目录
target_include_directories(${TARGET_NAME} PRIVATE
    ${OpenCASCADE_INCLUDE_DIR}
)

# 链接库
target_link_libraries(${TARGET_NAME}
    cad_core
    cad_sketch
    cad_feature
    cad_ui
    ${OpenCASCADE_LIBRARIES}
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
)

# 设置可执行文件属性
set_target_properties(${TARGET_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "JLiCAD"
)


if(WIN32)
   
endif()