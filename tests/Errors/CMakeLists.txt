add_executable(arangodbtests_errors
	main.cpp
	ErrorTTest.cpp)

target_include_directories(arangodbtests_errors PRIVATE
	${PROJECT_SOURCE_DIR}/lib)

target_link_libraries(arangodbtests_errors
	gtest)

