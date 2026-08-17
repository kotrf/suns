add_executable(suns_core_tests
    core_tests.cpp
)

target_link_libraries(suns_core_tests PRIVATE Suns::Core)

add_test(NAME suns_core_tests COMMAND suns_core_tests)

add_executable(suns_propulsion_tests
    propulsion_tests.cpp
)

target_link_libraries(suns_propulsion_tests PRIVATE Suns::Core)

add_test(NAME suns_propulsion_tests COMMAND suns_propulsion_tests)
