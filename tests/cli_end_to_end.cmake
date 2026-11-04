# Runs nmea-logger on the replay fixture and validates its outputs.
file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")

execute_process(
    COMMAND "${LOGGER}" --file "${FIXTURE}" --quiet
            --gpx "${WORKDIR}/ride.gpx" --geojson "${WORKDIR}/ride.geojson" --tee "${WORKDIR}/raw.nmea"
            --fence-circle "ne-corner:39.8920,32.7860,60"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "nmea-logger failed (${rc}): ${err}")
endif()

foreach(expected "EVENT ENTER ne-corner" "EVENT EXIT ne-corner" "fixes       314" "2 checksum")
    string(FIND "${out}" "${expected}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "missing '${expected}' in output:\n${out}")
    endif()
endforeach()

file(READ "${WORKDIR}/ride.gpx" gpx)
string(REGEX MATCHALL "<trkpt " trkpts "${gpx}")
list(LENGTH trkpts n)
if(NOT n EQUAL 314)
    message(FATAL_ERROR "expected 314 GPX track points, got ${n}")
endif()

file(READ "${WORKDIR}/ride.geojson" geojson)
foreach(expected "\"LineString\"" "\"kind\":\"geofence\"" "\"transition\":\"EXIT\"")
    string(FIND "${geojson}" "${expected}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "missing ${expected} in GeoJSON")
    endif()
endforeach()

# The tee recording must replay to the same result.
file(STRINGS "${WORKDIR}/raw.nmea" raw)
list(LENGTH raw raw_lines)
if(NOT raw_lines EQUAL 1368)
    message(FATAL_ERROR "expected 1368 recorded sentences, got ${raw_lines}")
endif()

execute_process(
    COMMAND "${LOGGER}" --file "${WORKDIR}/raw.nmea" --json -q
    RESULT_VARIABLE rc2
    OUTPUT_VARIABLE out2)
if(NOT rc2 EQUAL 0 OR NOT out2 MATCHES "\"fixes\":314")
    message(FATAL_ERROR "replay of tee recording failed: ${out2}")
endif()
message(STATUS "end-to-end OK")
