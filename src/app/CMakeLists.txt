qt_add_executable(dante-cli WIN32
    main.cpp
)

set_target_properties(dante-cli PROPERTIES
    OUTPUT_NAME "Dante CLI"
)

target_compile_definitions(dante-cli PRIVATE
    DANTE_VERSION_STRING="${DANTE_VERSION}"
    DANTE_BUILD_ID="${DANTE_BUILD_ID}"
)

target_link_libraries(dante-cli PRIVATE
    dante::core
    dante::platform
    dante::persistence
    dante::terminal
    dante::ui
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    Qt6::Network
    Qt6::Sql
)

# Embed resources (icons, themes, translations)
qt_add_resources(dante-cli "dante_app_resources"
    PREFIX "/"
    FILES
        ${CMAKE_SOURCE_DIR}/resources/icons/app.svg
        ${CMAKE_SOURCE_DIR}/resources/themes/dark.qss
)

if(WIN32)
    target_sources(dante-cli PRIVATE app.rc)
endif()

dante_apply_compile_options(dante-cli)

# Embed translations (pt-BR + en)
set(DANTE_TS_FILES
    ${CMAKE_SOURCE_DIR}/resources/translations/dante_pt_BR.ts
    ${CMAKE_SOURCE_DIR}/resources/translations/dante_en.ts
)

if(EXISTS ${CMAKE_SOURCE_DIR}/resources/translations/dante_pt_BR.ts)
    qt_add_translations(dante-cli
        TS_FILES ${DANTE_TS_FILES}
    )
endif()

install(TARGETS dante-cli RUNTIME DESTINATION bin)
