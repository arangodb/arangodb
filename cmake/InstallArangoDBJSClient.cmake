################################################################################
### @brief install client-side JavaScript files
################################################################################

install(
  DIRECTORY
    ${ARANGODB_SOURCE_DIR}/js/common
    ${ARANGODB_SOURCE_DIR}/js/client
  DESTINATION
    ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}
  FILES_MATCHING
    PATTERN "*.js"
  REGEX "^.*/js/client/modules/@arangodb/testsuites" EXCLUDE
  REGEX "^.*/js/client/modules/@arangodb/testutils" EXCLUDE
)

install(
  FILES
    ${ARANGODB_SOURCE_DIR}/js/JS_SHA1SUM.txt
  DESTINATION
    ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}
)

if (USE_ENTERPRISE)
  install(
    DIRECTORY
      ${ARANGODB_SOURCE_DIR}/enterprise/js/common
      ${ARANGODB_SOURCE_DIR}/enterprise/js/client
    DESTINATION    ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}
    FILES_MATCHING PATTERN "*.js"
  )
endif ()

# For the node modules we need all files except the following:
install(
  DIRECTORY ${ARANGODB_SOURCE_DIR}/js/node
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}
  REGEX "^.*/.bin"                                         EXCLUDE
  REGEX "^.*/.npmignore"                                   EXCLUDE
  REGEX "^.*/@eslint"                                      EXCLUDE
  REGEX "^.*/@eslint-community"                            EXCLUDE
  REGEX "^.*/@humanfs"                                     EXCLUDE
  REGEX "^.*/@humanwhocodes"                               EXCLUDE
  REGEX "^.*/@types"                                       EXCLUDE
  REGEX "^.*/@xmldom"                                      EXCLUDE
  REGEX "^.*/acorn"                                        EXCLUDE
  REGEX "^.*/acorn-jsx"                                    EXCLUDE
  REGEX "^.*/ansi_up"                                      EXCLUDE
  REGEX "^.*/callsites"                                    EXCLUDE
  REGEX "^.*/color-convert"                                EXCLUDE
  REGEX "^.*/color-name"                                   EXCLUDE
  REGEX "^.*/cross-spawn"                                  EXCLUDE
  REGEX "^.*/debug"                                        EXCLUDE
  REGEX "^.*/deep-is"                                      EXCLUDE
  REGEX "^.*/eslint"                                       EXCLUDE
  REGEX "^.*/eslint-scope"                                 EXCLUDE
  REGEX "^.*/eslint-visitor-keys"                          EXCLUDE
  REGEX "^.*/espree"                                       EXCLUDE
  REGEX "^.*/esquery"                                      EXCLUDE
  REGEX "^.*/esrecurse"                                    EXCLUDE
  REGEX "^.*/estraverse"                                   EXCLUDE
  REGEX "^.*/fast-json-stable-stringify"                   EXCLUDE
  REGEX "^.*/fast-levenshtein"                             EXCLUDE
  REGEX "^.*/file-entry-cache"                             EXCLUDE
  REGEX "^.*/find-up"                                      EXCLUDE
  REGEX "^.*/flat-cache"                                   EXCLUDE
  REGEX "^.*/flatted"                                      EXCLUDE
  REGEX "^.*/glob-parent"                                  EXCLUDE
  REGEX "^.*/globals"                                      EXCLUDE
  REGEX "^.*/has-flag"                                     EXCLUDE
  REGEX "^.*/ignore"                                       EXCLUDE
  REGEX "^.*/import-fresh"                                 EXCLUDE
  REGEX "^.*/imurmurhash"                                  EXCLUDE
  REGEX "^.*/is-extglob"                                   EXCLUDE
  REGEX "^.*/is-glob"                                      EXCLUDE
  REGEX "^.*/is-wsl"                                       EXCLUDE
  REGEX "^.*/isexe"                                        EXCLUDE
  REGEX "^.*/json-buffer"                                  EXCLUDE
  REGEX "^.*/json-stable-stringify-without-jsonify"        EXCLUDE
  REGEX "^.*/keyv"                                         EXCLUDE
  REGEX "^.*/levn"                                         EXCLUDE
  REGEX "^.*/locate-path"                                  EXCLUDE
  REGEX "^.*/lodash.merge"                                 EXCLUDE
  REGEX "^.*/natural-compare"                              EXCLUDE
  REGEX "^.*/node-netstat"                                 EXCLUDE
  REGEX "^.*/optionator"                                   EXCLUDE
  REGEX "^.*/p-limit"                                      EXCLUDE
  REGEX "^.*/p-locate"                                     EXCLUDE
  REGEX "^.*/parent-module"                                EXCLUDE
  REGEX "^.*/parse-prometheus-text-format"                 EXCLUDE
  REGEX "^.*/path-exists"                                  EXCLUDE
  REGEX "^.*/path-key"                                     EXCLUDE
  REGEX "^.*/prelude-ls"                                   EXCLUDE
  REGEX "^.*/resolve-from"                                 EXCLUDE
  REGEX "^.*/shallow-equal"                                EXCLUDE
  REGEX "^.*/shebang-command"                              EXCLUDE
  REGEX "^.*/shebang-regex"                                EXCLUDE
  REGEX "^.*/strip-json-comments"                          EXCLUDE
  REGEX "^.*/type-check"                                   EXCLUDE
  REGEX "^.*/uri-js"                                       EXCLUDE
  REGEX "^.*/which"                                        EXCLUDE
  REGEX "^.*/word-wrap"                                    EXCLUDE
  REGEX "^.*/yocto-queue"                                  EXCLUDE
)
