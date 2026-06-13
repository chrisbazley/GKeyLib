file(GLOB SOURCES "*.c")
file(GLOB PRIVATE_HEADERS "*.h")

add_executable(GKeyTests ${SOURCES} ${PRIVATE_HEADERS})
target_link_libraries(GKeyTests PRIVATE GKey)
add_test(NAME "GKeyTests" COMMAND GKeyTests)
