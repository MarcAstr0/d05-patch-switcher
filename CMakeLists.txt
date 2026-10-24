cmake_minimum_required(VERSION 3.22)
project(d05-patch-switcher VERSION 0.9.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(D05_BUILD_TESTS "Build the unit tests" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

# --- RtMidi (ALSA on Linux, CoreMIDI on macOS, WinMM on Windows), vendored at configure time ---
include(FetchContent)
set(RTMIDI_BUILD_STATIC_LIBS ON  CACHE BOOL "" FORCE)
set(RTMIDI_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(RTMIDI_BUILD_TESTING     OFF CACHE BOOL "" FORCE)
set(RTMIDI_API_JACK          OFF CACHE BOOL "" FORCE)
FetchContent_Declare(rtmidi
    GIT_REPOSITORY https://github.com/thestk/rtmidi.git
    GIT_TAG        6.0.0
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(rtmidi)

# --- Shared flags ---
add_library(d05_options INTERFACE)
target_compile_definitions(d05_options INTERFACE
    QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_compile_options(d05_options INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)
target_include_directories(d05_options INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- d05core: core + midi + app logic, no Widgets (RtMidi lives with the MIDI layer) ---
qt_add_library(d05core STATIC
    src/core/PatchAddress.h src/core/PatchAddress.cpp
    src/core/PatchCatalog.h src/core/PatchCatalog.cpp
    src/core/SearchQuery.h  src/core/SearchQuery.cpp
    src/core/Library.h      src/core/Library.cpp
    src/app/Paths.h         src/app/Paths.cpp
    src/app/AppSettings.h   src/app/AppSettings.cpp
    src/app/Cli.h           src/app/Cli.cpp
    src/midi/MidiBackend.h
    src/midi/FakeMidiBackend.h src/midi/FakeMidiBackend.cpp
    src/midi/MidiBackend.h src/midi/MidiBackend.cpp
    src/midi/RtMidiBackend.h src/midi/RtMidiBackend.cpp
    src/midi/MidiLink.h src/midi/MidiLink.cpp
)
# The preset table ships inside the binary as a Qt resource (:/data/d05-presets.tsv).
qt_add_resources(d05core "data"
    PREFIX "/"
    FILES data/d05-presets.tsv
)
target_link_libraries(d05core PUBLIC Qt6::Core d05_options PRIVATE rtmidi)

# --- d05ui: widgets ---
qt_add_library(d05ui STATIC
    src/ui/Theme.h          src/ui/Theme.cpp
    src/ui/GlyphTable.h     src/ui/GlyphTable.cpp
    src/ui/DisplayWidget.h  src/ui/DisplayWidget.cpp
    src/ui/SectionRule.h    src/ui/SectionRule.cpp
    src/ui/PanelButton.h    src/ui/PanelButton.cpp
    src/ui/PanelButtonField.h src/ui/PanelButtonField.cpp
    src/ui/Nameplate.h      src/ui/Nameplate.cpp
    src/ui/GroupBar.h       src/ui/GroupBar.cpp
    src/ui/BankGrid.h       src/ui/BankGrid.cpp
    src/ui/PatchListModel.h src/ui/PatchListModel.cpp
    src/ui/PatchDelegate.h  src/ui/PatchDelegate.cpp
    src/ui/PatchListView.h  src/ui/PatchListView.cpp
    src/ui/SettingsDialog.h src/ui/SettingsDialog.cpp
    src/ui/Shortcuts.h      src/ui/Shortcuts.cpp
    src/ui/ShortcutsDialog.h src/ui/ShortcutsDialog.cpp
    src/ui/AboutDialog.h    src/ui/AboutDialog.cpp
    src/ui/MainWindow.h     src/ui/MainWindow.cpp
    src/app/Screenshot.h    src/app/Screenshot.cpp
)
# Fonts, style sheet, arrows and icon ship inside the binary: :/fonts, :/theme, :/icons.
qt_add_resources(d05ui "ui"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    FILES
        assets/fonts/Inter-Regular.ttf
        assets/fonts/Inter-Medium.ttf
        assets/theme/app.qss
        assets/theme/arrow-down.png
        assets/theme/arrow-down@2x.png
        assets/icons/d05-patch-switcher.png
        assets/icons/d05-patch-switcher-16.png
        assets/icons/d05-patch-switcher-24.png
        assets/icons/d05-patch-switcher-32.png
        assets/icons/d05-patch-switcher-48.png
        assets/icons/d05-patch-switcher-64.png
        assets/icons/d05-patch-switcher-128.png
)
target_link_libraries(d05ui PUBLIC Qt6::Widgets d05core)

# --- Application ---
qt_add_executable(d05-patch-switcher src/app/main.cpp)
target_link_libraries(d05-patch-switcher PRIVATE d05ui d05core Qt6::Widgets)
target_compile_definitions(d05-patch-switcher PRIVATE D05_VERSION="${PROJECT_VERSION}")

set_target_properties(d05-patch-switcher PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "D-05 Patch Switcher"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.marcastr0.d05-patch-switcher"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "d05-patch-switcher.icns"
    MACOSX_BUNDLE_COPYRIGHT "MIT licence. Copyright 2026 Mario Castro Squella."
    MACOSX_BUNDLE_INFO_STRING "D-05 Patch Switcher ${PROJECT_VERSION}"
    MACOSX_BUNDLE_LONG_VERSION_STRING "D-05 Patch Switcher ${PROJECT_VERSION}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in")
if(APPLE)
    # Copy the icon into Contents/Resources; CFBundleIconFile names it.
    set(D05_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/d05-patch-switcher.icns")
    set_source_files_properties("${D05_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(d05-patch-switcher PRIVATE "${D05_ICNS}")
    # The licences travel inside the bundle (Contents/Resources).
    set(D05_NOTICES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${CMAKE_CURRENT_SOURCE_DIR}/THIRD-PARTY-NOTICES.md")
    set_source_files_properties(${D05_NOTICES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(d05-patch-switcher PRIVATE ${D05_NOTICES})
endif()
if(WIN32)
    # Icon and version resource (Explorer's Details tab), generated so the version follows the project.
    set(D05_ICO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/d05-patch-switcher.ico")
    configure_file(packaging/windows/d05-patch-switcher.rc.in "${CMAKE_CURRENT_BINARY_DIR}/d05-patch-switcher.rc" @ONLY)
    target_sources(d05-patch-switcher PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/d05-patch-switcher.rc")
endif()

install(TARGETS d05-patch-switcher
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin)
if(UNIX AND NOT APPLE)
    install(FILES packaging/d05-patch-switcher.desktop DESTINATION share/applications)
    install(FILES packaging/com.marcastr0.d05_patch_switcher.metainfo.xml DESTINATION share/metainfo)
    # Hicolor theme icons: 256 px is the unsuffixed master, the rest are -<size>.
    foreach(sz 16 24 32 48 64 128)
        install(FILES assets/icons/d05-patch-switcher-${sz}.png
                DESTINATION share/icons/hicolor/${sz}x${sz}/apps
                RENAME d05-patch-switcher.png)
    endforeach()
    install(FILES assets/icons/d05-patch-switcher.png
            DESTINATION share/icons/hicolor/256x256/apps)
    install(FILES LICENSE THIRD-PARTY-NOTICES.md DESTINATION share/doc/d05-patch-switcher)
endif()

# --- Tests ---
if(D05_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
