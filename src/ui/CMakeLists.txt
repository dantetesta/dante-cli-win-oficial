add_library(dante_ui STATIC
    main_window/MainWindow.cpp
    main_window/MainWindow.h
    tabs/TabBar.cpp
    tabs/TabBar.h
    sidebar/SidebarDock.cpp
    sidebar/SidebarDock.h
    toolbar/ToolbarWidget.cpp
    toolbar/ToolbarWidget.h
    settings/SettingsDialog.cpp
    settings/SettingsDialog.h
)

add_library(dante::ui ALIAS dante_ui)

target_include_directories(dante_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(dante_ui
    PUBLIC
        dante::core
        dante::platform
        dante::persistence
        dante::terminal
        Qt6::Widgets
        Qt6::Gui
        Qt6::Core
)

dante_apply_compile_options(dante_ui)
