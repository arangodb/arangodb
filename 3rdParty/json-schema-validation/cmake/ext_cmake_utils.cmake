#! some basic settings i use really often so lets have them in a macro

function(ext_log)
    message(STATUS "extINFO -- " ${ARGV})
endfunction(ext_log)

function(ext_fatal)
    message(FATAL_ERROR "FATAL ERROR -- " ${ARGV})
endfunction(ext_fatal)

function(ext_cat_file)
    if(UNIX)
        message(STATUS "extINFO -- @@@ content of: " ${ARGV})
        execute_process(COMMAND /bin/cat ${ARGV})
        message(STATUS "extINFO -- @@@ end content")
    endif()
endfunction(ext_cat_file)

#! prefix string with provided symbol(s) until is has given length
#
#  in_string - sting to be prefixed
#  length - desired length
#  fill - symbols(s) used for filling
#  out_string - this will hold the result
function(ext_prefix in_string length fill out_string)
    set(result "${in_string}")
    string(LENGTH "${in_string}" current_length)

    while(current_length LESS length)
        set(result "${fill}${result}")
        string(LENGTH "${result}" current_length)
    endwhile()

    set("${out_string}" "${result}" PARENT_SCOPE)
endfunction(ext_prefix)

#! this function acts like add_subdirectory but it checks
#  if CMakeLists.txt exists in the directory before adding it
function(ext_add_subdirectory dir debug)
    if(EXISTS "${dir}/CMakeLists.txt")
        add_subdirectory("${dir}")
        if(debug)
            message("adding directory ${dir}")
        endif()
    endif()
endfunction()

macro(ext_add_test_subdirectory type)
    set(EXT_TEST_TYPE "${type}")

    set(dir "${ARGV1}")
    if(dir STREQUAL "")
        set(dir "tests")
    endif()

    if("${type}" STREQUAL "google") # <-- expand type here!
        if(NOT TARGET gtest) #avoid recursive inclusion of gtest
          if("${LIBEXT_SOURCE_DIR}" STREQUAL "")
              ext_fatal("LIBEXT_SOURCE_DIR not found")
          endif()
          ext_log("using google test in: ${LIBEXT_SOURCE_DIR}/external_libs/googletest")
          add_subdirectory(${LIBEXT_SOURCE_DIR}/external_libs/googletest)
        endif()
    else()
        message(ERROR "unknown test type")
    endif()
    ext_log("adding tests in: ${dir}")
    add_subdirectory("${dir}")
endmacro(ext_add_test_subdirectory)
