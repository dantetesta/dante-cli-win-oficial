add_library(dante_core STATIC
    util/Result.h
    util/Uuid.cpp
    util/Uuid.h
    util/Logger.cpp
    util/Logger.h
    domain/Tab.cpp
    domain/Tab.h
    domain/Session.cpp
    domain/Session.h
    domain/Favorite.h
    domain/Snippet.h
    domain/Credential.h
    domain/AIProvider.h
    domain/SplitWorkspace.cpp
    domain/SplitWorkspace.h
    services/IShellLauncher.h
    services/ITabRepository.h
    parsing/AnsiParser.cpp
    parsing/AnsiParser.h
    parsing/AnsiTypes.h
)

add_library(dante::core ALIAS dante_core)

target_include_directories(dante_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(dante_core PUBLIC Qt6::Core)

dante_apply_compile_options(dante_core)
