add_library(dante_persistence STATIC
    config/AppConfig.cpp
    config/AppConfig.h
    schema/Migrations.cpp
    schema/Migrations.h
    repository/Database.cpp
    repository/Database.h
    repository/SqliteTabRepository.cpp
    repository/SqliteTabRepository.h
)

add_library(dante::persistence ALIAS dante_persistence)

target_include_directories(dante_persistence PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(dante_persistence
    PUBLIC dante::core Qt6::Core Qt6::Sql
)

# Embed SQL migrations as Qt resources
qt_add_resources(dante_persistence "dante_migrations"
    PREFIX "/sql"
    FILES
        schema/001_initial.sql
)

dante_apply_compile_options(dante_persistence)
