macro(UserWarning _msg)
   if("$ENV{DASHBOARD_TEST_FROM_CTEST}" STREQUAL "")
      # developer (non-dashboard) build
      message(WARNING "${_msg}")
   else()
      # dashboard build
      message(STATUS "${_msg}")
   endif()
endmacro()
