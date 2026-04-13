add_library(nmea_test_main STATIC test_main.cpp)
target_include_directories(nmea_test_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nmea_test_main PRIVATE nmea_warnings)

set(NMEA_FIXTURE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)

set(NMEA_UNIT_TESTS
    test_sentence
    test_parser
    test_geo
    test_time
    test_track
    test_stats
    test_geofence
    test_export
    test_io
    test_replay
    test_pipeline
    test_cli)

foreach(name IN LISTS NMEA_UNIT_TESTS)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE nmea nmea_test_main nmea_warnings)
    target_compile_definitions(${name} PRIVATE NMEA_FIXTURE_DIR="${NMEA_FIXTURE_DIR}")
    add_test(NAME ${name} COMMAND ${name})
endforeach()

# End-to-end: replay the fixture through the real binary and validate outputs.
add_test(NAME cli_replay_end_to_end
    COMMAND ${CMAKE_COMMAND}
        -DLOGGER=$<TARGET_FILE:nmea-logger>
        -DFIXTURE=${NMEA_FIXTURE_DIR}/metu_ride.nmea
        -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/e2e
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_end_to_end.cmake)
add_test(NAME cli_usage_error COMMAND nmea-logger --bogus)
set_tests_properties(cli_usage_error PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_help COMMAND nmea-logger --help)
set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: nmea-logger")
