set(TARGET_NAME cad_app)

# Source files
set(SOURCES
    src/main.cpp
)

# Create executable
add_executable(${TARGET_NAME} ${SOURCES})

# Include directories
target_include_directories(${TARGET_NAME} PRIVATE
    ${OpenCASCADE_INCLUDE_DIR}
)

# Link libraries
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

# Set executable properties
set_target_properties(${TARGET_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "JLiCAD"
)


if(WIN32)
   
endif()