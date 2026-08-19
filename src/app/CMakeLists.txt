qt_add_executable(suns_app
    main.cpp
    main_window.cpp
    main_window.hpp
    main_window_cargo.cpp
    main_window_dashboard.cpp
    main_window_map_modes.cpp
    main_window_planet.cpp
    main_window_save.cpp
    main_window_selection.cpp
    main_window_ui.cpp
    main_window_route_program.cpp
    route_program_dock.cpp
    route_program_dock.hpp
    save_game.cpp
    save_game.hpp
    ship_designer_dialog.cpp
    ship_designer_dialog.hpp
    star_item.cpp
    star_item.hpp
)

target_link_libraries(suns_app PRIVATE Qt6::Widgets Suns::Core)

set_target_properties(suns_app PROPERTIES
    OUTPUT_NAME "suns"
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

if(SUNS_BUILD_TESTS)
    add_test(NAME suns_ui_smoke COMMAND suns_app --smoke-test)
    set_tests_properties(suns_ui_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 10)
endif()
