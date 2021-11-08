# options
  
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION False)

message(STATUS "VelocyPack building examples: ${BuildVelocyPackExamples}")
if(BuildVelocyPackExamples)
    set(Examples
        exampleAliases
        exampleArrayHandling
        exampleArrayIterator
        exampleAttributeTranslation
        exampleBuilder
        exampleBuilderFancy
        exampleCustom
        exampleDumper
        exampleDumperFile
        exampleDumperPretty
        exampleObjectHandling
        exampleObjectIterator
        exampleObjectLookup
        exampleParser
        exampleSlice
    )

    foreach (exampleName ${Examples})
        add_executable(${exampleName} ${exampleName}.cpp)
        target_link_libraries(${exampleName} velocypack)
    endforeach()
endif()
