#--------------------------------------------------------------------------------
# include this macro to dump all variables:
# include("cmake/dump_vars.cmake")

message(STATUS "------------ BEGINN DUMP CMAKE VARS ---------------")
get_property(_vars DIRECTORY PROPERTY COMPILE_DEFINITIONS)
message(STATUS "------------ DUMP COMPILE_DEFINITIONS --------------")
foreach (_var ${_vars})
    message(STATUS "${_var}")
endforeach()

get_property(_vars DIRECTORY PROPERTY MACROS)
message(STATUS "------------ DUMP MACROS --------------")
foreach (_var ${_vars})
    message(STATUS "${_var}")
endforeach()

get_property(_vars DIRECTORY PROPERTY VARIABLES)
message(STATUS "------------ DUMP VARIABLES --------------")
foreach (_var ${_vars})
    message(STATUS "${_var}=${${_var}}")
endforeach()

message(STATUS "------------ END DUMP CMAKE VARS --------------")
#--------------------------------------------------------------------------------
