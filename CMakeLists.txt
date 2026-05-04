cmake_minimum_required(VERSION 3.16)
project(BadgeEditorQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
find_package(qtadvanceddocking REQUIRED)

set(SOURCES
    main.cpp
    mainwindow.cpp
    designerwidget.cpp
    batchlayoutdialog.cpp
    badgeitem.cpp
    badgegraphicitem.cpp
    guideitem.cpp
    lightingeffect.cpp
    layoutengine.cpp
    imageprocessor.cpp
)

set(HEADERS
    mainwindow.h
    designerwidget.h
    batchlayoutdialog.h
    badgeitem.h
    badgegraphicitem.h
    guideitem.h
    lightingeffect.h
    layoutengine.h
    imageprocessor.h
)

add_executable(BadgeEditorQt ${SOURCES} ${HEADERS})

target_link_libraries(BadgeEditorQt PRIVATE Qt6::Widgets Qt6::Core qtadvanceddocking)

target_include_directories(BadgeEditorQt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
