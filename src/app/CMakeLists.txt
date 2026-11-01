add_library(suns_app_ui STATIC
    galaxy_setup_widget.cpp
    galaxy_setup_widget.hpp
    main_window_fleet_operations.cpp
    main_window.cpp
    main_window_campaign.cpp
    main_window_races.cpp
    race_file.cpp
    race_file.hpp
    race_setup_widget.cpp
    race_setup_widget.hpp
    main_window.hpp
    main_window_cargo.cpp
    main_window_communications.cpp
    main_window_dashboard.cpp
    main_window_fleet_portrait.cpp
    main_window_fleet_management.cpp
    main_window_fleet_summary.cpp
    main_window_colony_workspace.cpp
    main_window_history.cpp
    main_window_layout.cpp
    main_window_map_modes.cpp
    main_window_map_labels.cpp
    map_marker_items.cpp
    map_marker_items.hpp
    main_window_mining.cpp
    main_window_stations.cpp
    main_window_intelligence.cpp
    main_window_strategic_operations.cpp
    main_window_planet.cpp
    main_window_production.cpp
    main_window_colony_templates.cpp
    colony_plan_combo.hpp
    main_window_readability.cpp
    main_window_research.cpp
    main_window_save.cpp
    main_window_selection.cpp
    main_window_turn_messages.cpp
    main_window_wormholes.cpp
    main_window_unknown_objects.cpp
    main_window_science_samples.cpp
    main_window_exotic_materials.cpp
    main_window_visitors.cpp
    main_window_survey_probes.cpp
    main_window_interstellar_medium.cpp
    main_window_outposts.cpp
    main_window_mobile_bases.cpp
    main_window_transport_lines.cpp
    main_window_orbital_bases.cpp
    main_window_planetary_engineering.cpp
    main_window_ui.cpp
    toolbar_icons.cpp
    toolbar_icons.hpp
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

target_link_libraries(suns_app_ui PUBLIC Qt6::Widgets Suns::Core)
target_include_directories(suns_app_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_executable(suns_app main.cpp)
target_link_libraries(suns_app PRIVATE suns_app_ui)

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
