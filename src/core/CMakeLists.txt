add_library(suns_core
    src/campaign.cpp
    src/game_state.cpp
    src/hulls.cpp
    src/communications.cpp
    src/mineral_economy.cpp
    src/mining.cpp
    src/player_knowledge.cpp
    src/production_forecast.cpp
    src/production.cpp
    src/terraforming.cpp
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
