
if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    set(CMAKE_CXX_FLAGS "${CAMAKE_CXX_FLAGS} -std=c++2a -fconcepts")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL lang)
    set(CMAKE_CXX_FLAGS "${CAMAKE_CXX_FLAGS} -std=c++2a -fconcepts")
else()
    message(FATAL_ERROR "compiler not supported")
endif()
