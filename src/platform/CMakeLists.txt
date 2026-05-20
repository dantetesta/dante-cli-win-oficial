add_library(dante_platform STATIC
    pty/IPtyBackend.h
    pty/PtyEvents.h
    process/ShellResolver.cpp
    process/ShellResolver.h
    memstat/ProcessStats.cpp
    memstat/ProcessStats.h
    fs/PathUtils.cpp
    fs/PathUtils.h
)

if(WIN32)
    target_sources(dante_platform PRIVATE
        pty/conpty/WindowsHandle.h
        pty/conpty/ConPtyBackend.cpp
        pty/conpty/ConPtyBackend.h
    )
    target_link_libraries(dante_platform PRIVATE
        psapi
        userenv
    )
endif()

add_library(dante::platform ALIAS dante_platform)

target_include_directories(dante_platform
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(dante_platform
    PUBLIC dante::core Qt6::Core
)

dante_apply_compile_options(dante_platform)
