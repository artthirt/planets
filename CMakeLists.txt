cmake_minimum_required(VERSION 3.22)

set(PROJECT_NAME Planets)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Bin)

project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL Widgets)

set(SRC
    data.qrc
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    space_scene.cpp
    space_scene.h
    shaders.cpp
    shaders.h
    meshes.cpp
    meshes.h
    bodies.h
    camera.h
)

add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets Qt6::OpenGL
)
