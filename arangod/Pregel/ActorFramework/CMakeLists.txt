add_library(arangodb_actors STATIC Actor.cpp Runtime.cpp)
target_link_libraries(arangodb_actors fmt)
