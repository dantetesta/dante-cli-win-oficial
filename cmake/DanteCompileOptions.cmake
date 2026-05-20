function(dante_apply_compile_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /Zc:preprocessor
            /utf-8
            /MP
            /EHsc
            $<$<CONFIG:Release>:/O2 /Oi /Ob2 /GL /Gw>
            $<$<CONFIG:Debug>:/Od /Zi /RTC1>
        )
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
        )
        target_compile_definitions(${target} PRIVATE
            _UNICODE
            UNICODE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _CRT_SECURE_NO_WARNINGS
            _WIN32_WINNT=0x0A00
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wconversion
            -Wsign-conversion
            -Wnull-dereference
            -Wdouble-promotion
            $<$<CONFIG:Release>:-O2 -fno-plt>
            $<$<CONFIG:Debug>:-O0 -g3>
        )
        if(WIN32)
            target_compile_definitions(${target} PRIVATE
                _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00
            )
        endif()
    endif()

    set_target_properties(${target} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ON
    )
endfunction()
