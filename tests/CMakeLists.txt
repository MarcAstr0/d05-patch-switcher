find_package(Qt6 6.4 REQUIRED COMPONENTS Test)

# One executable per test file. Add new tests with d05_add_test(<name> <source>...).
function(d05_add_test name)
    qt_add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE Qt6::Test d05core)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

d05_add_test(test_presets_table test_presets_table.cpp)
d05_add_test(test_patch_address test_patch_address.cpp)
d05_add_test(test_patch_catalog test_patch_catalog.cpp)
d05_add_test(test_search_query test_search_query.cpp)
d05_add_test(test_library test_library.cpp)
