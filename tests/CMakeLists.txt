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

add_executable(suns_logistics_tests
    logistics_tests.cpp
)

target_link_libraries(suns_logistics_tests PRIVATE Suns::Core)

add_test(NAME suns_logistics_tests COMMAND suns_logistics_tests)

add_executable(suns_ship_designer_tests
    ship_designer_tests.cpp
)

target_link_libraries(suns_ship_designer_tests PRIVATE Suns::Core)

add_test(NAME suns_ship_designer_tests COMMAND suns_ship_designer_tests)

add_executable(suns_arrival_actions_tests
    arrival_actions_tests.cpp
)

target_link_libraries(suns_arrival_actions_tests PRIVATE Suns::Core)

add_test(NAME suns_arrival_actions_tests COMMAND suns_arrival_actions_tests)
