# -*- mode: CMAKE; -*-

set(MAN_NAMES
  man1/arangob.1
  man1/arangodump.1
  man1/arangoimp.1
  man1/arangorestore.1
  man1/arangosh.1
  man8/rcarangod.8
  man8/arangod.8
  man8/arango-dfdb.8
  man8/foxx-manager.8 
)

set(MAN_FILES)

foreach (m IN LISTS MAN_NAMES)
  set(msrc ${CMAKE_SOURCE_DIR}/Documentation/${m})
  set(mdst ${CMAKE_SOURCE_DIR}/Documentation/man/${m})

  add_custom_command(
    OUTPUT
      ${mdst}
    COMMAND
      ${CMAKE_SOURCE_DIR}/utils/manPages.sh ${msrc} ${mdst} ${ARANGODB_VERSION}
    DEPENDS
      ${msrc}
    WORKING_DIRECTORY
      ${CMAKE_BINARY_DIR}
    COMMENT
      "Building manpage ${mdst}"
    VERBATIM
  )

  list(APPEND MAN_FILES ${mdst})
endforeach ()

add_custom_target(man ALL DEPENDS ${MAN_FILES})
