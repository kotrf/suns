add_library(suns_core
    src/campaign.cpp
    src/computer_player.cpp
    src/computer_military.cpp
    src/computer_stargates.cpp
    src/game_state.cpp
    src/hulls.cpp
    src/equipment.cpp
    src/stations.cpp
    src/deep_space.cpp
    src/mobile_base.cpp
    src/combat.cpp
    src/strategic_operations.cpp
    src/communications.cpp
    src/emission_interception.cpp
    src/mineral_economy.cpp
    src/mining.cpp
    src/player_knowledge.cpp
    src/reconnaissance.cpp
    src/production_forecast.cpp
    src/production.cpp
    src/planetary_recovery.cpp
    src/unknown_objects.cpp
    src/science_samples.cpp
    src/exotic_materials.cpp
    src/interstellar_medium.cpp
    src/terraforming.cpp
    src/planetary_engineering.cpp
    src/propulsion.cpp
    src/scanners.cpp
    src/radiation.cpp
    src/turn_processor.cpp
    src/wormholes.cpp
)

add_library(Suns::Core ALIAS suns_core)

target_include_directories(suns_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(suns_core PUBLIC cxx_std_20)
