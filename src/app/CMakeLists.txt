qt_add_executable(suns_app
    main.cpp
    main_window.cpp
    main_window.hpp
    main_window_route_program.cpp
    route_program_dock.cpp
    route_program_dock.hpp
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
