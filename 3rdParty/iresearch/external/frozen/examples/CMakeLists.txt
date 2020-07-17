set(frozen.examples.srcs enum_to_string enum_to_string_hash pixel_art static_assert)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang")
    list(APPEND frozen.examples.srcs html_entities_map)
endif()

foreach(src IN LISTS frozen.examples.srcs)
    add_executable(frozen.example.${src} ${src}.cpp)
    target_link_libraries(frozen.example.${src} PUBLIC frozen::frozen)
endforeach()

if (TARGET frozen.example.html_entities_map)
    target_compile_options(frozen.example.html_entities_map PUBLIC
        $<$<CXX_COMPILER_ID:Clang>:-fconstexpr-steps=123456789>
        $<$<CXX_COMPILER_ID:AppleClang>:-fconstexpr-steps=123456789>)
endif()
