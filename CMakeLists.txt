cmake_minimum_required(VERSION 3.5)

project(IBMFFontEditor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Freetype REQUIRED)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        bitmapRenderer.cpp
        bitmapRenderer.h
        setPixelCommand.h
        setPixelCommand.cpp
        actionButton.cpp
        actionButton.h
        IBMFDriver/IBMFFontMod.cpp
        IBMFDriver/IBMFFontMod.hpp
        IBMFDriver/RLEGenerator.hpp
        IBMFDriver/RLEExtractor.hpp
        IBMFDriver/IBMFDefs.hpp
        Unicode/UBlocks.hpp
        Unicode/uBlockSelectionDialog.hpp
        blocksDialog.cpp
        blocksDialog.h
        mainwindow.ui
        blocksDialog.ui
        fontParameterDialog.h
        fontParameterDialog.cpp
        fontParameterDialog.ui
        freeType.h
        freeType.cpp
        characterViewer.cpp
        characterViewer.h
        characterSelector.cpp
        characterSelector.h
        Kerning/kerningDialog.cpp
        Kerning/kerningDialog.h
        Kerning/kerningModel.cpp
        Kerning/kerningModel.h
        Kerning/kerningDelegate.cpp
        Kerning/kerningDelegate.h
        Kerning/kerningEditor.cpp
        Kerning/kerningEditor.h
        Kerning/kerningItem.cpp
        Kerning/kerningItem.h
        Kerning/kerningRenderer.cpp
        Kerning/kerningRenderer.h
        Kerning/kernEntry.h
        proofingDialog.h
        proofingDialog.cpp
        proofingDialog.ui
        drawingSpace.h
        drawingSpace.cpp
        fix16Delegate.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(IBMFFontEditor
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET IBMFFontEditor APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(IBMFFontEditor SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(IBMFFontEditor
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(IBMFFontEditor PRIVATE Freetype::Freetype  Qt${QT_VERSION_MAJOR}::Widgets)

set_target_properties(IBMFFontEditor PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(IBMFFontEditor)
endif()
