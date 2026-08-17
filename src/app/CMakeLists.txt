qt_add_executable(suns_app
    main.cpp
    main_window.cpp
    main_window.hpp
    star_item.cpp
    star_item.hpp
)

target_link_libraries(suns_app PRIVATE Qt6::Widgets Suns::Core)

set_target_properties(suns_app PROPERTIES
    OUTPUT_NAME "suns"
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
