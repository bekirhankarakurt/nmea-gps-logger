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
    test_pipeline)

foreach(name IN LISTS NMEA_UNIT_TESTS)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE nmea nmea_test_main nmea_warnings)
    target_compile_definitions(${name} PRIVATE NMEA_FIXTURE_DIR="${NMEA_FIXTURE_DIR}")
    add_test(NAME ${name} COMMAND ${name})
endforeach()

