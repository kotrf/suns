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

add_executable(suns_route_program_tests
    route_program_tests.cpp
)

target_link_libraries(suns_route_program_tests PRIVATE Suns::Core)

add_test(NAME suns_route_program_tests COMMAND suns_route_program_tests)

add_executable(suns_mineral_economy_tests
    mineral_economy_tests.cpp
)

target_link_libraries(suns_mineral_economy_tests PRIVATE Suns::Core)

add_test(NAME suns_mineral_economy_tests COMMAND suns_mineral_economy_tests)


add_executable(suns_communications_tests
    communications_tests.cpp
)

target_link_libraries(suns_communications_tests PRIVATE Suns::Core)

add_test(NAME suns_communications_tests COMMAND suns_communications_tests)

add_executable(suns_player_knowledge_tests
    player_knowledge_tests.cpp
)

target_link_libraries(suns_player_knowledge_tests PRIVATE Suns::Core)

add_test(NAME suns_player_knowledge_tests COMMAND suns_player_knowledge_tests)

add_executable(suns_propulsion_tests
    propulsion_tests.cpp
)

target_link_libraries(suns_propulsion_tests PRIVATE Suns::Core)

add_test(NAME suns_propulsion_tests COMMAND suns_propulsion_tests)

add_executable(suns_arrival_actions_tests
    arrival_actions_tests.cpp
)

target_link_libraries(suns_arrival_actions_tests PRIVATE Suns::Core)

add_test(NAME suns_arrival_actions_tests COMMAND suns_arrival_actions_tests)

add_executable(suns_logistics_tests
    logistics_tests.cpp
)

target_link_libraries(suns_logistics_tests PRIVATE Suns::Core)

add_test(NAME suns_logistics_tests COMMAND suns_logistics_tests)

add_executable(suns_waypoint_program_tests
    waypoint_program_tests.cpp
)

target_link_libraries(suns_waypoint_program_tests PRIVATE Suns::Core)

add_test(NAME suns_waypoint_program_tests COMMAND suns_waypoint_program_tests)

add_executable(suns_ship_designer_tests
    ship_designer_tests.cpp
)

target_link_libraries(suns_ship_designer_tests PRIVATE Suns::Core)

add_test(NAME suns_ship_designer_tests COMMAND suns_ship_designer_tests)

add_executable(suns_research_tests
    research_tests.cpp
)

target_link_libraries(suns_research_tests PRIVATE Suns::Core)

add_test(NAME suns_research_tests COMMAND suns_research_tests)

add_executable(suns_remote_mining_tests
    remote_mining_tests.cpp
)

target_link_libraries(suns_remote_mining_tests PRIVATE Suns::Core)

add_test(NAME suns_remote_mining_tests COMMAND suns_remote_mining_tests)

add_executable(suns_fleet_management_tests
    fleet_management_tests.cpp
)

target_link_libraries(suns_fleet_management_tests PRIVATE Suns::Core)

add_test(NAME suns_fleet_management_tests COMMAND suns_fleet_management_tests)

add_executable(suns_moving_fleet_target_tests
    moving_fleet_target_tests.cpp
)

target_link_libraries(suns_moving_fleet_target_tests PRIVATE Suns::Core)

add_test(NAME suns_moving_fleet_target_tests COMMAND suns_moving_fleet_target_tests)

add_executable(suns_interception_tests
    interception_tests.cpp
)

target_link_libraries(suns_interception_tests PRIVATE Suns::Core)

add_test(NAME suns_interception_tests COMMAND suns_interception_tests)

add_executable(suns_production_queue_tests
    production_queue_tests.cpp
)

target_link_libraries(suns_production_queue_tests PRIVATE Suns::Core)

add_test(NAME suns_production_queue_tests COMMAND suns_production_queue_tests)

add_executable(suns_orbital_station_tests
    orbital_station_tests.cpp
)

target_link_libraries(suns_orbital_station_tests PRIVATE Suns::Core)

add_test(NAME suns_orbital_station_tests COMMAND suns_orbital_station_tests)

add_executable(suns_history_tests
    history_tests.cpp
)

target_link_libraries(suns_history_tests PRIVATE Suns::Core)

add_test(NAME suns_history_tests COMMAND suns_history_tests)

add_executable(suns_race_profile_tests
    race_profile_tests.cpp
)

target_link_libraries(suns_race_profile_tests PRIVATE Suns::Core)

add_test(NAME suns_race_profile_tests COMMAND suns_race_profile_tests)

add_executable(suns_variable_star_tests
    variable_star_tests.cpp
)

target_link_libraries(suns_variable_star_tests PRIVATE Suns::Core)

add_test(NAME suns_variable_star_tests COMMAND suns_variable_star_tests)

add_executable(suns_precursor_artifact_tests
    precursor_artifact_tests.cpp
)

target_link_libraries(suns_precursor_artifact_tests PRIVATE Suns::Core)

add_test(NAME suns_precursor_artifact_tests COMMAND suns_precursor_artifact_tests)

if(SUNS_BUILD_APP)
    add_executable(suns_save_game_tests
        save_game_tests.cpp
        ../src/app/save_game.cpp
        ../src/app/save_game.hpp
    )
    target_include_directories(suns_save_game_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
    target_link_libraries(suns_save_game_tests PRIVATE Qt6::Core Suns::Core)
    add_test(NAME suns_save_game_tests COMMAND suns_save_game_tests)
endif()
