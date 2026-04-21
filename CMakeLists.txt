cmake_minimum_required(VERSION 3.16)
project(JLi_CAD VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# find OpenCASCADE 7.8
find_package(OpenCASCADE REQUIRED
    PATHS "C:/Program Files/OCCT"
    NO_DEFAULT_PATH
)

# find Qt5
set(Qt5_DIR "C:/qt/Qt5.9.7/5.9.7/msvc2017_64/lib/cmake/Qt5")
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui)


# set Qt5 autoMOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# include directories
include_directories(${OpenCASCADE_INCLUDE_DIR})

# add subdirectory
add_subdirectory(cad_core)
add_subdirectory(cad_sketch)
add_subdirectory(cad_feature)
add_subdirectory(cad_ui)
add_subdirectory(cad_app)

# set Visual Studio startup project
if(MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT cad_app)
endif()