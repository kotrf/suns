add_executable(suns_core_tests
    core_tests.cpp
)

target_link_libraries(suns_core_tests PRIVATE Suns::Core)

add_test(NAME suns_core_tests COMMAND suns_core_tests)

add_executable(suns_radiation_tests
    radiation_tests.cpp
)

target_link_libraries(suns_radiation_tests PRIVATE Suns::Core)

add_test(NAME suns_radiation_tests COMMAND suns_radiation_tests)

add_executable(suns_cargo_tests
    cargo_tests.cpp
)

target_link_libraries(suns_cargo_tests PRIVATE Suns::Core)

add_test(NAME suns_cargo_tests COMMAND suns_cargo_tests)
