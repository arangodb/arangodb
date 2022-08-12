# Add Analyze with CppCheck target if CppCheck is installed
find_program(CMAKE_CXX_CPPCHECK cppcheck NAMES cppcheck HINTS $ENV{PROGRAMFILES}/cppcheck)
  
if(CMAKE_CXX_CPPCHECK)
  # Check CppCheck version
  set(CPP_CHECK_CMD ${CMAKE_CXX_CPPCHECK} --version)
    execute_process(COMMAND ${CPP_CHECK_CMD}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      RESULT_VARIABLE CPP_CHECK_RESULT
      OUTPUT_VARIABLE CPP_CHECK_VERSION
      ERROR_VARIABLE CPP_CHECK_ERROR)
    
  # Check if version could be extracted
  if(CPP_CHECK_RESULT EQUAL 0)
    # Get number of CPU cores
    include(ProcessorCount)
    ProcessorCount(CPU_CORES)

    # Append desired arguments to CppCheck
    list(
      APPEND CMAKE_CXX_CPPCHECK
        # 
	"--xml" "--xml-version=2"
          
        # Using the below template will allow jumping to any found error from inside Visual Studio output window by double click
        "--template=\"{file}({line}): {severity} ({id}): {message}\""
          
        # Use all the available CPU cores
        "-j ${CPU_CORES}"

        # Ignore 3rdParty code
        "-i${CMAKE_SOURCE_DIR}/3rdParty"

        # Only show found errors
        "--quiet"
          
        # Desired warning level in CppCheck
        "--enable=warning,performance,portability,missingInclude"
          
        # Optional: Specified C++ version
        "--std=c++20"
           
        # Optional: Specified platform
        "--platform=unix64"
          
        # Optional: suppression file stored in same directory as the top level CMake script
        "--suppressions-list=${CMAKE_SOURCE_DIR}/cppcheck_suppressions.txt"
          
        # Optional: Use inline suppressions
        "--inline-suppr"
          
        # Run CppCheck from the working directory, as specified in the add_custom_target command below
      "--project=${CMAKE_BINARY_DIR}/compile_commands.json"

      # Legacy from old script; unclear why this is required.
      "-DUSE_PLAN_CACHE"
      "-DDEFINE_FACTORY_DEFAULT"
        )
      
    add_custom_target(cppcheck
     COMMAND ${CMAKE_CXX_CPPCHECK} 2> "cppcheck.xml"
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT "Static code analysis using ${CPP_CHECK_VERSION}"
    )
  endif()
endif()

