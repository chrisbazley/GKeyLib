find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
find_program(UCSIM_Z80_EXECUTABLE NAMES ucsim_z80)
if(NOT UCSIM_Z80_EXECUTABLE)
    message(FATAL_ERROR "SDCC tests require ucsim_z80 on PATH")
endif()

add_executable(GKeyTests Main.c ../GKeyCompTest.c ../GKeyDecompTest.c
    ../RingBufferTest.c ../GKeyRoundTripTest.c ../TestData.c "${cbutil_SOURCE_DIR}/tests/sdcc/runtime.c")
target_compile_definitions(GKeyTests PRIVATE TEST_FUNCTION=GKey_tests)
target_link_libraries(GKeyTests PRIVATE GKey CBUtil)
# Reserve 0xc000..0xdeff for static data, below the simulator and fixed heap.
# The round-trip arrays exceed the shared runtime's original 1 KiB region.
target_link_options(GKeyTests PRIVATE --data-loc 0xc000)
add_test(NAME GKeyTests
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run.py"
        "${cbutil_SOURCE_DIR}/tests/sdcc/run.py"
        "${UCSIM_Z80_EXECUTABLE}" "$<TARGET_FILE:GKeyTests>")
set_tests_properties(GKeyTests PROPERTIES TIMEOUT 65)
