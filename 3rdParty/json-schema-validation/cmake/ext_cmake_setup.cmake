# TODO - test this macro in other libs
# execute macro only in top-level CMakeLists.txt
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
    include(ext_cmake_utils)
    include(ext_cmake_compiler_specific)

    # execute this setup just once
    if(NOT EXT_SETUP_DONE)
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
            set(LINUX TRUE)
        else()
            set(LINUX FALSE)
        endif()

        set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
        # set / modify default install prefix
        if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
            ext_log("install location defaulted")
            if(UNIX)
                ##set(CMAKE_INSTALL_PREFIX  "$ENV{HOME}/.local")
            else()
                # do not change the default for other operating systems
            endif()
        else()
            ext_log("install location manually provided")
        endif()
        ext_log("installing to: ${CMAKE_INSTALL_PREFIX}")

        ext_define_warning_flags()

        set(EXT_CXX_COMPILER_IS_GCC FALSE)
        set(EXT_CXX_COMPILER_IS_CLANG FALSE)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            set(EXT_CXX_COMPILER_IS_GCC TRUE)
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            set(EXT_CXX_COMPILER_IS_CLANG TRUE)
        endif()

        set(EXT_SETUP_DONE TRUE)
    endif()
endif()
