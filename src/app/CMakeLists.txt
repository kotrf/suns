add_library(suns_app_ui STATIC
    application_icon.cpp
    application_icon.hpp
    galaxy_setup_widget.cpp
    galaxy_setup_widget.hpp
    ship_fitting_layout.cpp
    ship_fitting_layout.hpp
    ship_catalog_info.cpp
    ship_catalog_info.hpp
    technology_catalog_dialog.cpp
    technology_catalog_dialog.hpp
    main_window_technology_catalog.cpp
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
    station_details_dialog.cpp
    station_details_dialog.hpp
    main_window_intelligence.cpp
    main_window_strategic_operations.cpp
    mineral_stock_panel.cpp
    mineral_stock_panel.hpp
    main_window_planet.cpp
    main_window_production.cpp
    simple_production_estimate.hpp
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

set(suns_icon_files)
foreach(icon_size 16 24 32 48 64 128 256)
    list(APPEND suns_icon_files "resources/icons/suns-${icon_size}.png")
endforeach()
qt_add_resources(suns_app_ui suns_icons
    PREFIX "/suns"
    BASE "resources"
    FILES ${suns_icon_files}
)

qt_add_executable(suns_app main.cpp)
target_link_libraries(suns_app PRIVATE suns_app_ui)

if(WIN32)
    enable_language(RC)
    set(SUNS_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/suns.ico")
    configure_file(resources/suns.rc.in "${CMAKE_CURRENT_BINARY_DIR}/suns.rc" @ONLY)
    target_sources(suns_app PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/suns.rc")
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/suns.rc"
        PROPERTIES OBJECT_DEPENDS "${SUNS_ICON_FILE}")
endif()

set_target_properties(suns_app PROPERTIES
    OUTPUT_NAME "suns"
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

include(GNUInstallDirs)
install(TARGETS suns_app
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    BUNDLE DESTINATION .)
if(UNIX AND NOT APPLE)
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/linux/suns.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES resources/icons/suns.svg
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
    foreach(icon_size 16 24 32 48 64 128 256)
        install(FILES "resources/icons/suns-${icon_size}.png"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${icon_size}x${icon_size}/apps"
            RENAME suns.png)
    endforeach()
endif()

if(SUNS_BUILD_TESTS)
    add_test(NAME suns_ui_smoke COMMAND suns_app --smoke-test)
    set_tests_properties(suns_ui_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 10)
endif()
