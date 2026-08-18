add_library(suns_core
    src/game_state.cpp
    src/radiation.cpp
    src/turn_processor.cpp
)

add_library(Suns::Core ALIAS suns_core)

target_include_directories(suns_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(suns_core PUBLIC cxx_std_20)
