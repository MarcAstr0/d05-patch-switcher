find_package(Qt6 6.4 REQUIRED COMPONENTS Test)

# One executable per test file. Add new tests with d05_add_test(<name> <source>...).
function(d05_add_test name)
    qt_add_executable(${name} ${ARGN} ${CMAKE_CURRENT_SOURCE_DIR}/unbuffered_stdio.cpp)
    target_link_libraries(${name} PRIVATE Qt6::Test d05core)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

d05_add_test(test_presets_table test_presets_table.cpp)
d05_add_test(test_patch_address test_patch_address.cpp)
d05_add_test(test_patch_catalog test_patch_catalog.cpp)
d05_add_test(test_search_query test_search_query.cpp)
d05_add_test(test_library test_library.cpp)
d05_add_test(test_midi_link test_midi_link.cpp)
d05_add_test(test_cli test_cli.cpp)
d05_add_test(test_rtmidi_init test_rtmidi_init.cpp)
d05_add_test(test_cli_process test_cli_process.cpp)
target_compile_definitions(test_cli_process PRIVATE D05_BINARY="$<TARGET_FILE:d05-patch-switcher>")
add_dependencies(test_cli_process d05-patch-switcher)

d05_add_test(test_display test_display.cpp)
target_link_libraries(test_display PRIVATE d05ui Qt6::Widgets)
target_compile_definitions(test_display PRIVATE D05_BINARY="$<TARGET_FILE:d05-patch-switcher>"
                                                 D05_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_dependencies(test_display d05-patch-switcher)

d05_add_test(test_main_window test_main_window.cpp)
target_link_libraries(test_main_window PRIVATE d05ui Qt6::Widgets)

# Hardware-free MIDI check against ALSA "Midi Through" (exit 77 = skipped).
if(UNIX AND NOT APPLE)
    add_test(NAME midi_smoke COMMAND ${CMAKE_SOURCE_DIR}/scripts/midi-smoke.sh $<TARGET_FILE:d05-patch-switcher>)
    set_tests_properties(midi_smoke PROPERTIES SKIP_RETURN_CODE 77 LABELS "hardware-free" TIMEOUT 60)
endif()

d05_add_test(test_library_views test_library_views.cpp)
target_link_libraries(test_library_views PRIVATE d05ui Qt6::Widgets)

d05_add_test(test_app_settings test_app_settings.cpp)
d05_add_test(test_settings_ui test_settings_ui.cpp)
target_link_libraries(test_settings_ui PRIVATE d05ui Qt6::Widgets)

# The real RtMidi input path against ALSA "Midi Through" (exit 77 = skipped).
if(UNIX AND NOT APPLE)
    d05_add_test(test_rtmidi_follow test_rtmidi_follow.cpp)
    target_link_libraries(test_rtmidi_follow PRIVATE rtmidi)
    set_tests_properties(test_rtmidi_follow PROPERTIES SKIP_RETURN_CODE 77 LABELS "hardware-free" TIMEOUT 60)
endif()

d05_add_test(test_help_ui test_help_ui.cpp)
target_link_libraries(test_help_ui PRIVATE d05ui Qt6::Widgets)
