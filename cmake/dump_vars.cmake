#--------------------------------------------------------------------------------
# include this macro to dump all variables:
# include("cmake/dump_vars.cmake")
get_cmake_property(_variableNames VARIABLES)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()
#--------------------------------------------------------------------------------
