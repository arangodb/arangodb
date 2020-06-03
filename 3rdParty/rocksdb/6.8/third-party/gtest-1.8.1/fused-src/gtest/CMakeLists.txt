add_library(gtest gtest-all.cc)
add_library(gtest_main gtest_main.cc)

if(MSVC)
    target_compile_options(gtest
      PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MT>"
      PRIVATE "/D _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING"
    )

    target_compile_options(gtest_main
      PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MT>"
      PRIVATE "/D _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING"
    )
endif()
