add_library(dante_terminal STATIC
    engine/TerminalBuffer.cpp
    engine/TerminalBuffer.h
    engine/Scrollback.cpp
    engine/Scrollback.h
    engine/ColorPalette.cpp
    engine/ColorPalette.h
    widget/TerminalWidget.cpp
    widget/TerminalWidget.h
    registry/TerminalRegistry.cpp
    registry/TerminalRegistry.h
)

add_library(dante::terminal ALIAS dante_terminal)

target_include_directories(dante_terminal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(dante_terminal
    PUBLIC dante::core dante::platform Qt6::Widgets Qt6::Gui Qt6::Core
)

dante_apply_compile_options(dante_terminal)
