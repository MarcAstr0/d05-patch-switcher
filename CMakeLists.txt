cmake_minimum_required(VERSION 3.22)
project(d05-patch-switcher VERSION 0.1.0 LANGUAGES CXX)

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

# --- Application ---
qt_add_executable(d05-patch-switcher
    src/main.cpp
)

# The preset table ships inside the binary as a Qt resource (:/data/d05-presets.tsv).
qt_add_resources(d05-patch-switcher "data"
    PREFIX "/"
    FILES data/d05-presets.tsv
)

target_link_libraries(d05-patch-switcher PRIVATE Qt6::Widgets rtmidi)
target_compile_definitions(d05-patch-switcher PRIVATE
    D05_VERSION="${PROJECT_VERSION}"
    QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_compile_options(d05-patch-switcher PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)

set_target_properties(d05-patch-switcher PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "D-05 Patch Switcher"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.marcastr0.d05-patch-switcher"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")

install(TARGETS d05-patch-switcher
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin)
if(UNIX AND NOT APPLE)
    install(FILES packaging/d05-patch-switcher.desktop DESTINATION share/applications)
endif()

# --- Tests ---
if(D05_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
