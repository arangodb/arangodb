# header only atm
# if(AR_USE_SYSTEM_LIBS)
#     find_package(Boost REQUIED)
# else()
#     #mimic findpackage
#     set(Boost_INCLUDE_DIRS "1.58.0")
#     set(Boost_VERSION "1.58.0")
#     set(Boost_FOUND True)
# endif()

# add_library(ar_boost INTERFACE)
# target_include_directories(ar_boost INTERFACE ${Boost_INCLUDE_DIRS})
