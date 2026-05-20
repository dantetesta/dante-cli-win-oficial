qt_add_executable(test_ansi_parser
    test_ansi_parser.cpp
)

target_link_libraries(test_ansi_parser PRIVATE
    dante::core
    Qt6::Test
)

add_test(NAME ansi_parser COMMAND test_ansi_parser)

qt_add_executable(test_terminal_buffer
    test_terminal_buffer.cpp
)

target_link_libraries(test_terminal_buffer PRIVATE
    dante::core
    Qt6::Test
)

add_test(NAME terminal_buffer COMMAND test_terminal_buffer)
