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
  REGEX "^.*/@babel"                                       EXCLUDE
  REGEX "^.*/@xmldom"                                      EXCLUDE
  REGEX "^.*/acorn"                                        EXCLUDE
  REGEX "^.*/acorn-jsx"                                    EXCLUDE
  REGEX "^.*/ansi-escapes"                                 EXCLUDE
  REGEX "^.*/ansi_up"                                      EXCLUDE
  REGEX "^.*/astral-regex"                                 EXCLUDE
  REGEX "^.*/callsites"                                    EXCLUDE
  REGEX "^.*/chardet"                                      EXCLUDE
  REGEX "^.*/cli-cursor"                                   EXCLUDE
  REGEX "^.*/cli-width"                                    EXCLUDE
  REGEX "^.*/color-convert"                                EXCLUDE
  REGEX "^.*/color-name"                                   EXCLUDE
  REGEX "^.*/cross-spawn"                                  EXCLUDE
  REGEX "^.*/debug"                                        EXCLUDE
  REGEX "^.*/deep-is"                                      EXCLUDE
  REGEX "^.*/doctrine"                                     EXCLUDE
  REGEX "^.*/emoji-regex"                                  EXCLUDE
  REGEX "^.*/eslint"                                       EXCLUDE
  REGEX "^.*/eslint-scope"                                 EXCLUDE
  REGEX "^.*/eslint-utils"                                 EXCLUDE
  REGEX "^.*/eslint-visitor-keys"                          EXCLUDE
  REGEX "^.*/espree"                                       EXCLUDE
  REGEX "^.*/esquery"                                      EXCLUDE
  REGEX "^.*/esrecurse"                                    EXCLUDE
  REGEX "^.*/estraverse"                                   EXCLUDE
  REGEX "^.*/external-editor"                              EXCLUDE
  REGEX "^.*/fast-json-stable-stringify"                   EXCLUDE
  REGEX "^.*/fast-levenshtein"                             EXCLUDE
  REGEX "^.*/figures"                                      EXCLUDE
  REGEX "^.*/file-entry-cache"                             EXCLUDE
  REGEX "^.*/flat-cache"                                   EXCLUDE
  REGEX "^.*/flatted"                                      EXCLUDE
  REGEX "^.*/fs.realpath"                                  EXCLUDE
  REGEX "^.*/functional-red-black-tree"                    EXCLUDE
  REGEX "^.*/glob"                                         EXCLUDE
  REGEX "^.*/globals"                                      EXCLUDE
  REGEX "^.*/has-flag"                                     EXCLUDE
  REGEX "^.*/ignore"                                       EXCLUDE
  REGEX "^.*/import-fresh"                                 EXCLUDE
  REGEX "^.*/imurmurhash"                                  EXCLUDE
  REGEX "^.*/inflight"                                     EXCLUDE
  REGEX "^.*/inquirer"                                     EXCLUDE
  REGEX "^.*/is-fullwidth-code-point"                      EXCLUDE
  REGEX "^.*/is-wsl"                                       EXCLUDE
  REGEX "^.*/isexe"                                        EXCLUDE
  REGEX "^.*/json-stable-stringify-without-jsonify"        EXCLUDE
  REGEX "^.*/levn"                                         EXCLUDE
  REGEX "^.*/mimic-fn"                                     EXCLUDE
  REGEX "^.*/minimist"                                     EXCLUDE
  REGEX "^.*/mkdirp"                                       EXCLUDE
  REGEX "^.*/mute-stream"                                  EXCLUDE
  REGEX "^.*/natural-compare"                              EXCLUDE
  REGEX "^.*/nice-try"                                     EXCLUDE
  REGEX "^.*/node-netstat"                                 EXCLUDE
  REGEX "^.*/once"                                         EXCLUDE
  REGEX "^.*/onetime"                                      EXCLUDE
  REGEX "^.*/optionator"                                   EXCLUDE
  REGEX "^.*/os-tmpdir"                                    EXCLUDE
  REGEX "^.*/parent-module"                                EXCLUDE
  REGEX "^.*/parse-prometheus-text-format"                 EXCLUDE
  REGEX "^.*/path-is-absolute"                             EXCLUDE
  REGEX "^.*/path-is-inside"                               EXCLUDE
  REGEX "^.*/path-key"                                     EXCLUDE
  REGEX "^.*/picocolors"                                   EXCLUDE
  REGEX "^.*/prelude-ls"                                   EXCLUDE
  REGEX "^.*/progress"                                     EXCLUDE
  REGEX "^.*/regexpp"                                      EXCLUDE
  REGEX "^.*/resolve-from"                                 EXCLUDE
  REGEX "^.*/restore-cursor"                               EXCLUDE
  REGEX "^.*/rimraf"                                       EXCLUDE
  REGEX "^.*/run-async"                                    EXCLUDE
  REGEX "^.*/rxjs"                                         EXCLUDE
  REGEX "^.*/shallow-equal"                                EXCLUDE
  REGEX "^.*/shebang-command"                              EXCLUDE
  REGEX "^.*/shebang-regex"                                EXCLUDE
  REGEX "^.*/signal-exit"                                  EXCLUDE
  REGEX "^.*/slice-ansi"                                   EXCLUDE
  REGEX "^.*/string-width"                                 EXCLUDE
  REGEX "^.*/strip-json-comments"                          EXCLUDE
  REGEX "^.*/table"                                        EXCLUDE
  REGEX "^.*/text-table"                                   EXCLUDE
  REGEX "^.*/through"                                      EXCLUDE
  REGEX "^.*/tmp"                                          EXCLUDE
  REGEX "^.*/tslib"                                        EXCLUDE
  REGEX "^.*/type-check"                                   EXCLUDE
  REGEX "^.*/uri-js"                                       EXCLUDE
  REGEX "^.*/which"                                        EXCLUDE
  REGEX "^.*/word-wrap"                                    EXCLUDE
  REGEX "^.*/wrappy"                                       EXCLUDE
  REGEX "^.*/write"                                        EXCLUDE
)
