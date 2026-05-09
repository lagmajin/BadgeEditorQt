cmake_minimum_required(VERSION 3.28)
project(BadgeEditorQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core PrintSupport OpenGLWidgets)
find_package(Qt6 COMPONENTS Svg QUIET)
find_package(qtadvanceddocking-qt6 REQUIRED)
find_package(OpenImageIO CONFIG REQUIRED)

set(SOURCES
    main.cpp
    mainwindow.cpp
    dockstylemanager.cpp
    designerwidget.cpp
    layoutworkspacewidget.cpp
    viewportbackend.cpp
    batchlayoutdialog.cpp
    mixedlayoutdialog.cpp
    exportdialog.cpp
    printdialog.cpp
    appsettingsdialog.cpp
    transferdebugdialog.cpp
    badgeitem.cpp
    badgegraphicitem.cpp
    guideitem.cpp
    lightingeffect.cpp
    layoutengine.cpp
    imageprocessor.cpp
    projectsync.cpp
)

set(HEADERS
    constants.h
    mainwindow.h
    dockstylemanager.h
    designerwidget.h
    layoutworkspacewidget.h
    viewportbackend.h
    batchlayoutdialog.h
    mixedlayoutdialog.h
    exportdialog.h
    printdialog.h
    appsettingsdialog.h
    transferdebugdialog.h
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

target_link_libraries(BadgeEditorQt PRIVATE Qt6::Widgets Qt6::Core Qt6::PrintSupport Qt6::OpenGLWidgets ads::qtadvanceddocking-qt6 OpenImageIO::OpenImageIO)

if (WIN32)
    target_link_libraries(BadgeEditorQt PRIVATE dwmapi ole32 windowscodecs)
endif()

if (TARGET Qt6::Svg)
    target_link_libraries(BadgeEditorQt PRIVATE Qt6::Svg)
    target_compile_definitions(BadgeEditorQt PRIVATE BADGEEDITOR_HAS_QTSVG=1)
endif()

target_include_directories(BadgeEditorQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ArtifactStudio/third_party
)

if (MSVC)
    target_compile_options(BadgeEditorQt PRIVATE /wd4616)
endif()

set(BADGEEDITOR_SOURCE_DIR_CMAKE "${CMAKE_CURRENT_SOURCE_DIR}")
file(TO_CMAKE_PATH "${BADGEEDITOR_SOURCE_DIR_CMAKE}" BADGEEDITOR_SOURCE_DIR_CMAKE)
target_compile_definitions(BadgeEditorQt PRIVATE BADGEEDITOR_SOURCE_DIR="${BADGEEDITOR_SOURCE_DIR_CMAKE}")

if (WIN32 AND DEFINED Qt6_DIR)
    get_filename_component(BADGEEDITOR_QT_INSTALL_PREFIX "${Qt6_DIR}/../.." ABSOLUTE)
    set(BADGEEDITOR_QT_RELEASE_PLUGINS_DIR "${BADGEEDITOR_QT_INSTALL_PREFIX}/Qt6/plugins")
    set(BADGEEDITOR_QT_DEBUG_PLUGINS_DIR "${BADGEEDITOR_QT_INSTALL_PREFIX}/debug/Qt6/plugins")

    if (NOT EXISTS "${BADGEEDITOR_QT_DEBUG_PLUGINS_DIR}")
        set(BADGEEDITOR_QT_DEBUG_PLUGINS_DIR "${BADGEEDITOR_QT_RELEASE_PLUGINS_DIR}")
    endif()

    if (EXISTS "${BADGEEDITOR_QT_RELEASE_PLUGINS_DIR}")
        add_custom_command(TARGET BadgeEditorQt POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "$<IF:$<CONFIG:Debug>,${BADGEEDITOR_QT_DEBUG_PLUGINS_DIR},${BADGEEDITOR_QT_RELEASE_PLUGINS_DIR}>"
                "$<TARGET_FILE_DIR:BadgeEditorQt>"
            COMMENT "Copying Qt runtime plugins"
            VERBATIM
        )
    endif()
endif()
