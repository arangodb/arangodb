cmake_minimum_required( VERSION 3.0.2 )

project( date_prj )

find_package( Threads REQUIRED )

enable_testing( )

option( USE_SYSTEM_TZ_DB "Use the operating system's timezone database" OFF )
option( USE_TZ_DB_IN_DOT "Save the timezone database in the current folder" OFF )
option( BUILD_TZ_STATIC  "Build a static version of library" ON )

function( print_option OPT )
	if ( NOT DEFINED PRINT_OPTION_CURR_${OPT} OR ( NOT PRINT_OPTION_CURR_${OPT} STREQUAL ${OPT} ) )
		set( PRINT_OPTION_CURR_${OPT} ${${OPT}} CACHE BOOL "" )
		mark_as_advanced(PRINT_OPTION_CURR_${OPT})
		message( "# date: ${OPT} ${${OPT}}" )
	endif( )
endfunction( )

print_option( USE_SYSTEM_TZ_DB )
print_option( USE_TZ_DB_IN_DOT )
print_option( BUILD_TZ_STATIC  )

if( USE_SYSTEM_TZ_DB )
	add_definitions( -DUSE_AUTOLOAD=0 )
	add_definitions( -DHAS_REMOTE_API=0 )
	# cannot set USE_OS_TZDB to 1 on Windows
	if( NOT WIN32 )
		add_definitions( -DUSE_OS_TZDB=1 )
	endif( )
else( )
	add_definitions( -DUSE_AUTOLOAD=1 )
	add_definitions( -DHAS_REMOTE_API=1 )
	add_definitions( -DUSE_OS_TZDB=0 )
	find_package( CURL REQUIRED )
	include_directories( SYSTEM ${CURL_INCLUDE_DIRS} )
	set( OPTIONAL_LIBRARIES ${CURL_LIBRARIES} )
endif( )

if( USE_TZ_DB_IN_DOT )
	add_definitions( -DINSTALL=. )
endif( )

set( HEADER_FOLDER "include" )
set( SOURCE_FOLDER "src" )
set( TEST_FOLDER "test" )

# This is needed so IDE's live MSVC show header files
set( HEADER_FILES
	${HEADER_FOLDER}/date/chrono_io.h
	${HEADER_FOLDER}/date/date.h
	${HEADER_FOLDER}/date/ios.h
	${HEADER_FOLDER}/date/islamic.h
	${HEADER_FOLDER}/date/iso_week.h
	${HEADER_FOLDER}/date/julian.h
	${HEADER_FOLDER}/date/tz.h
	${HEADER_FOLDER}/date/tz_private.h
)

if( BUILD_TZ_STATIC )
	add_library( tz STATIC ${HEADER_FILES} ${SOURCE_FOLDER}/tz.cpp )
else( )
	add_library( tz SHARED ${HEADER_FILES} ${SOURCE_FOLDER}/tz.cpp )
endif( )

if ( ${CMAKE_MINOR_VERSION} GREATER 7 )
  set( TZ_CXX_STANDARD 17 )
else( )
  set( TZ_CXX_STANDARD 14 )
endif( )

set_property(TARGET tz PROPERTY CXX_STANDARD ${TZ_CXX_STANDARD})
target_link_libraries( tz ${CMAKE_THREAD_LIBS_INIT} ${OPTIONAL_LIBRARIES} )

# add include folders to the library and targets that consume it
target_include_directories(tz PUBLIC
    $<BUILD_INTERFACE:
        ${CMAKE_CURRENT_SOURCE_DIR}/${HEADER_FOLDER}
    >
    $<INSTALL_INTERFACE:
        include
    >
)

add_library(date_interface INTERFACE) # an interface (not a library), to enable automatic include_directory (for when just date.h, but not "tz.h and its lib" are needed)

# add include folders to the INTERFACE and targets that consume it
target_include_directories(date_interface INTERFACE
    $<BUILD_INTERFACE:
        ${CMAKE_CURRENT_SOURCE_DIR}/${HEADER_FOLDER}
    >
    $<INSTALL_INTERFACE:
        include
    >
)


install( TARGETS tz DESTINATION lib )
install( DIRECTORY ${HEADER_FOLDER}/ DESTINATION include/ )

add_custom_target( testit COMMAND ${CMAKE_CTEST_COMMAND} )

add_dependencies( testit tz )
function( add_pass_tests TEST_GLOB TEST_PREFIX )
	file( GLOB_RECURSE FILENAMES ${TEST_GLOB} )
	include_directories( "${HEADER_FOLDER}/date" )

	foreach( TEST_FILE ${FILENAMES} )
		get_filename_component( TEST_NAME ${TEST_FILE} NAME_WE )
		get_filename_component( TEST_EXT ${TEST_FILE} EXT )
		if( NOT ${TEST_EXT} STREQUAL ".fail.cpp" )
			set( PREFIX "${TEST_PREFIX}_pass_${TEST_NAME}" )
			set( BIN_NAME ${PREFIX}_bin )
			set( TST_NAME ${PREFIX}_test )
			add_executable( ${BIN_NAME} EXCLUDE_FROM_ALL ${TEST_FILE} )
			set_property(TARGET ${BIN_NAME} PROPERTY CXX_STANDARD ${TZ_CXX_STANDARD})
			add_test( ${TST_NAME} ${BIN_NAME} )
			target_link_libraries( ${BIN_NAME} tz )
			add_dependencies( testit ${BIN_NAME} )
		endif( )
	endforeach( )
endfunction( )

function( add_fail_tests TEST_GLOB TEST_PREFIX )
	file( GLOB_RECURSE FILENAMES ${TEST_GLOB} )

	foreach( TEST_FILE ${FILENAMES} )
		get_filename_component( TEST_NAME ${TEST_FILE} NAME_WE )
		get_filename_component( TEST_EXT ${TEST_FILE} EXT )

			set( TEST_TYPE "_fail" )

		set( PREFIX "${TEST_PREFIX}_fail_${TEST_NAME}" )
		set( BIN_NAME ${PREFIX}_bin )
		set( TST_NAME ${PREFIX}_test )

		#target_compile_definitions( ${BIN_NAME} PRIVATE ${TST_NAME} )
		set( TEST_BIN_NAME ${CMAKE_BINARY_DIR}/${BIN_NAME} )
		add_custom_target( ${BIN_NAME}
			COMMAND ${PROJECT_SOURCE_DIR}/compile_fail.sh ${TEST_BIN_NAME} ${CMAKE_CXX_COMPILER} -std=c++17 -L${CMAKE_BINARY_DIR}/ -ltz -I${PROJECT_SOURCE_DIR}/${HEADER_FOLDER}/date -o ${BIN_NAME} ${TEST_FILE}
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			COMMENT ${TST_NAME}
			)
		add_test( ${TST_NAME} "${PROJECT_SOURCE_DIR}/test_fail.sh" ${CMAKE_BINARY_DIR}/${BIN_NAME} WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/" )
		#set_tests_properties( ${TST_NAME} PROPERTIES WILL_FAIL TRUE)
		add_dependencies( testit ${BIN_NAME} )
	endforeach( )
endfunction( )

file( GLOB children RELATIVE "${PROJECT_SOURCE_DIR}/${TEST_FOLDER}" "${PROJECT_SOURCE_DIR}/${TEST_FOLDER}/*" )
foreach( child ${children} )
	if( IS_DIRECTORY "${PROJECT_SOURCE_DIR}/${TEST_FOLDER}/${child}" )
		set( CUR_FOLDER "${PROJECT_SOURCE_DIR}/${TEST_FOLDER}/${child}" )
		add_pass_tests( "${CUR_FOLDER}/*.cpp" ${child} )
		if( NOT WIN32 )
			add_fail_tests( "${CUR_FOLDER}/*.fail.cpp" ${child} )
		endif( )
	endif( )
endforeach( )

