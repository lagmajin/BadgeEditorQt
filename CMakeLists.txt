cmake_minimum_required(VERSION 3.28)
project(BadgeEditorQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
find_package(qtadvanceddocking-qt6 REQUIRED)
find_package(OpenImageIO CONFIG REQUIRED)

set(SOURCES
    main.cpp
    mainwindow.cpp
    designerwidget.cpp
    layoutworkspacewidget.cpp
    batchlayoutdialog.cpp
    badgeitem.cpp
    badgegraphicitem.cpp
    guideitem.cpp
    lightingeffect.cpp
    layoutengine.cpp
    imageprocessor.cpp
    projectsync.cpp
)

set(HEADERS
    mainwindow.h
    designerwidget.h
    layoutworkspacewidget.h
    batchlayoutdialog.h
    badgeitem.h
    badgegraphicitem.h
    guideitem.h
    lightingeffect.h
    layoutengine.h
    imageprocessor.h
    projectsync.h
)

add_executable(BadgeEditorQt ${SOURCES} ${HEADERS})

target_sources(BadgeEditorQt
    PRIVATE
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
            badge.paper.ixx
            badge.model.ixx
            badge.layout.ixx
            badge.imageio.ixx
            badge.document.ixx
            badge.documentio.ixx
            badge.qtbridge.ixx
)

target_link_libraries(BadgeEditorQt PRIVATE Qt6::Widgets Qt6::Core ads::qtadvanceddocking-qt6 OpenImageIO::OpenImageIO)

target_include_directories(BadgeEditorQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ArtifactStudio/third_party
)

if (MSVC)
    target_compile_options(BadgeEditorQt PRIVATE /wd4616)
endif()
