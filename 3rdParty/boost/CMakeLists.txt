project(boost)

#the boost version must also be adjusted in the top-level CMakeLists.txt file
set(boost_version "1.71.0")
set(BOOST_VERSION ${boost_version} PARENT_SCOPE)
set(boost_src "${CMAKE_CURRENT_SOURCE_DIR}/${boost_version}")

#create interface target for boost header only libraries
message(STATUS "using 3rdParty BOOST")
add_library(boost_boost INTERFACE)
target_include_directories(boost_boost SYSTEM INTERFACE "${boost_src}")

add_library(boost_filesystem STATIC
    ${boost_src}/libs/filesystem/src/codecvt_error_category.cpp
    ${boost_src}/libs/filesystem/src/operations.cpp
    ${boost_src}/libs/filesystem/src/path.cpp
    ${boost_src}/libs/filesystem/src/path_traits.cpp
    ${boost_src}/libs/filesystem/src/portability.cpp
    ${boost_src}/libs/filesystem/src/unique_path.cpp
    ${boost_src}/libs/filesystem/src/utf8_codecvt_facet.cpp
    ${boost_src}/libs/filesystem/src/windows_file_codecvt.cpp
)
target_link_libraries(boost_filesystem PUBLIC boost_boost) #this will add required includes
target_compile_definitions(boost_filesystem PRIVATE "BOOST_SYSTEM_STATIC_LINK=1")
target_compile_definitions(boost_filesystem PUBLIC  "BOOST_ALL_NO_LIB=1")

if(MSVC)
  list(APPEND BOOST_LOCALE_PLATFORM_DEFINE "BOOST_LOCALE_NO_POSIX_BACKEND=1")
  list(APPEND BOOST_LOCALE_PLATFORM_SRC
    ${boost_src}/libs/locale/src/win32/collate.cpp
    ${boost_src}/libs/locale/src/win32/converter.cpp
    ${boost_src}/libs/locale/src/win32/lcid.cpp
    ${boost_src}/libs/locale/src/win32/numeric.cpp
    ${boost_src}/libs/locale/src/win32/win_backend.cpp
  )
elseif(APPLE)
  list(APPEND BOOST_LOCALE_PLATFORM_DEFINE "BOOST_LOCALE_NO_WINAPI_BACKEND=1" "BOOST_LOCALE_WITH_ICU=1") # do not use 'iconv' on MacOS, hence must use ICU instead
  list(APPEND BOOST_LOCALE_PLATFORM_SRC
    ${boost_src}/libs/locale/src/posix/codecvt.cpp
    ${boost_src}/libs/locale/src/posix/collate.cpp
    ${boost_src}/libs/locale/src/posix/converter.cpp
    ${boost_src}/libs/locale/src/posix/numeric.cpp
    ${boost_src}/libs/locale/src/posix/posix_backend.cpp
  )
else()
  list(APPEND BOOST_LOCALE_PLATFORM_DEFINE "BOOST_LOCALE_NO_WINAPI_BACKEND=1" "BOOST_LOCALE_WITH_ICONV=1")
  list(APPEND BOOST_LOCALE_PLATFORM_SRC
    ${boost_src}/libs/locale/src/posix/codecvt.cpp
    ${boost_src}/libs/locale/src/posix/collate.cpp
    ${boost_src}/libs/locale/src/posix/converter.cpp
    ${boost_src}/libs/locale/src/posix/numeric.cpp
    ${boost_src}/libs/locale/src/posix/posix_backend.cpp
  )
endif()

#build boost_locale - add more source files as needed
add_library(boost_locale STATIC
    ${boost_src}/libs/locale/src/encoding/codepage.cpp
    ${boost_src}/libs/locale/src/icu/boundary.cpp
    ${boost_src}/libs/locale/src/icu/codecvt.cpp
    ${boost_src}/libs/locale/src/icu/collator.cpp
    ${boost_src}/libs/locale/src/icu/conversion.cpp
    ${boost_src}/libs/locale/src/icu/date_time.cpp
    ${boost_src}/libs/locale/src/icu/formatter.cpp
    ${boost_src}/libs/locale/src/icu/icu_backend.cpp
    ${boost_src}/libs/locale/src/icu/numeric.cpp
    ${boost_src}/libs/locale/src/icu/time_zone.cpp
    ${boost_src}/libs/locale/src/shared/date_time.cpp
    ${boost_src}/libs/locale/src/shared/format.cpp
    ${boost_src}/libs/locale/src/shared/formatting.cpp
    ${boost_src}/libs/locale/src/shared/generator.cpp
    ${boost_src}/libs/locale/src/shared/ids.cpp
    ${boost_src}/libs/locale/src/shared/localization_backend.cpp
    ${boost_src}/libs/locale/src/shared/message.cpp
    ${boost_src}/libs/locale/src/shared/mo_lambda.cpp
    ${boost_src}/libs/locale/src/std/codecvt.cpp
    ${boost_src}/libs/locale/src/std/collate.cpp
    ${boost_src}/libs/locale/src/std/converter.cpp
    ${boost_src}/libs/locale/src/std/numeric.cpp
    ${boost_src}/libs/locale/src/std/std_backend.cpp
    ${boost_src}/libs/locale/src/util/codecvt_converter.cpp
    ${boost_src}/libs/locale/src/util/default_locale.cpp
    ${boost_src}/libs/locale/src/util/gregorian.cpp
    ${boost_src}/libs/locale/src/util/info.cpp
    ${boost_src}/libs/locale/src/util/locale_data.cpp
    ${BOOST_LOCALE_PLATFORM_SRC}
)
add_dependencies(boost_locale v8_build) # for ICU libs
target_link_libraries(boost_locale PUBLIC boost_boost) #this will add required includes
target_include_directories(boost_locale SYSTEM PRIVATE ${ICU_INCLUDE_DIR})
target_compile_definitions(boost_locale PRIVATE "BOOST_SYSTEM_STATIC_LINK=1")
target_compile_definitions(boost_locale PUBLIC  "BOOST_ALL_NO_LIB=1")

if(APPLE)
  target_link_libraries(boost_locale PUBLIC boost_thread) # must link against boost_thread due to use of ICU on MacOS
endif()

foreach(ELEMENT ${BOOST_LOCALE_PLATFORM_DEFINE})
  target_compile_definitions(boost_locale PRIVATE "${ELEMENT}")
endforeach()

#build boost_program_options - add more source files as needed
add_library(boost_program_options STATIC
    ${boost_src}/libs/program_options/src/cmdline.cpp
    ${boost_src}/libs/program_options/src/config_file.cpp
    ${boost_src}/libs/program_options/src/convert.cpp
    ${boost_src}/libs/program_options/src/options_description.cpp
    ${boost_src}/libs/program_options/src/parsers.cpp
    ${boost_src}/libs/program_options/src/positional_options.cpp
    ${boost_src}/libs/program_options/src/split.cpp
    ${boost_src}/libs/program_options/src/utf8_codecvt_facet.cpp
    ${boost_src}/libs/program_options/src/value_semantic.cpp
    ${boost_src}/libs/program_options/src/variables_map.cpp
    ${boost_src}/libs/program_options/src/winmain.cpp
)
target_link_libraries(boost_program_options PUBLIC boost_boost) #this will add required includes
target_compile_definitions(boost_program_options PRIVATE "BOOST_SYSTEM_STATIC_LINK=1")
target_compile_definitions(boost_program_options PUBLIC  "BOOST_ALL_NO_LIB=1")

#build boost_system - there seems to be just one cpp file:)
add_library(boost_system STATIC
    ${boost_src}/libs/system/src/error_code.cpp
)
target_link_libraries(boost_system PUBLIC boost_boost) #this will add required includes
target_compile_definitions(boost_system PRIVATE "BOOST_SYSTEM_STATIC_LINK=1")
target_compile_definitions(boost_system PUBLIC  "BOOST_ALL_NO_LIB=1")

if(MSVC)
  list(APPEND BOOST_THREAD_PLATFORM_SRC
    ${boost_src}/libs/thread/src/win32/thread.cpp
    ${boost_src}/libs/thread/src/win32/tss_dll.cpp
    ${boost_src}/libs/thread/src/win32/tss_pe.cpp
  )
else()
  list(APPEND BOOST_THREAD_PLATFORM_SRC
    ${boost_src}/libs/thread/src/pthread/once.cpp
    ${boost_src}/libs/thread/src/pthread/once_atomic.cpp
    ${boost_src}/libs/thread/src/pthread/thread.cpp
  )
endif()

#build boost_thread - add more source files as needed
add_library(boost_thread STATIC
    ${boost_src}/libs/thread/src/future.cpp
    ${boost_src}/libs/thread/src/tss_null.cpp
    ${BOOST_THREAD_PLATFORM_SRC}
)
target_link_libraries(boost_thread PUBLIC boost_boost) #this will add required includes
target_link_libraries(boost_locale PUBLIC boost_thread)
target_compile_definitions(boost_thread PRIVATE "BOOST_SYSTEM_STATIC_LINK=1")
target_compile_definitions(boost_thread PUBLIC  "BOOST_ALL_NO_LIB=1")
