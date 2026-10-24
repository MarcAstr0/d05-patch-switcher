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
    src/midi/PortLister.h   src/midi/PortLister.cpp
)
# The preset table ships inside the binary as a Qt resource (:/data/d05-presets.tsv).
qt_add_resources(d05core "data"
    PREFIX "/"
    FILES data/d05-presets.tsv
)
target_link_libraries(d05core PUBLIC Qt6::Core d05_options PRIVATE rtmidi)

# --- d05ui: widgets ---
qt_add_library(d05ui STATIC
    src/ui/ScaffoldWindow.h src/ui/ScaffoldWindow.cpp
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
