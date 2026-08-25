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
