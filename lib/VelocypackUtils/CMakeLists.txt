add_library(velocypack_utils STATIC
  VelocyPackStringLiteral.cpp)  
target_link_libraries(velocypack_utils PUBLIC
  velocypack)
