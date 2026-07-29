################################################################################
### @brief install client-side JavaScript files
################################################################################

# Test-only modules are stripped from non-maintainer (release) installs;
# maintainer builds keep them so CI can run the test framework against an
# install tree. Deliberately NOT stripped, despite the test-y names:
#   - jsunity, @arangodb/testrunner, @arangodb/mocha-runner: wired into
#     "arangosh --javascript.unit-tests" (V8ShellFeature.cpp) and used by
#     release-test-automation against installed packages
#   - @arangodb/testutils/seededRandom.js: required by rta-makedata
#   - @arangodb/mocha*, @arangodb/foxx/test-utils: the Foxx service test
#     framework, a runtime feature ("foxx test")
set(ARANGODB_JS_CLIENT_TEST_EXCLUDES "")
if (NOT USE_MAINTAINER_MODE)
  set(ARANGODB_JS_CLIENT_TEST_EXCLUDES
    REGEX "^.*/js/client/modules/@arangodb/(test-helper|aql-helper)\\.js$" EXCLUDE
    REGEX "^.*/js/common/modules/@arangodb/test-helper-common\\.js$"       EXCLUDE
    REGEX "^.*/js/common/modules/@arangodb/test-generators"                EXCLUDE
    REGEX "^.*/js/common/modules/@arangodb/testutils"                      EXCLUDE
    REGEX "^.*/js/common/modules/@arangodb/graph/(graphs-generation|helpers)\\.js$" EXCLUDE)
endif ()

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
  ${ARANGODB_JS_CLIENT_TEST_EXCLUDES}
)

if (NOT USE_MAINTAINER_MODE)
  # The one file rta-makedata needs out of the otherwise test-only
  # js/common/modules/@arangodb/testutils directory excluded above.
  install(
    FILES ${ARANGODB_SOURCE_DIR}/js/common/modules/@arangodb/testutils/seededRandom.js
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}/common/modules/@arangodb/testutils
  )
endif ()

# JS_SHA1SUM.txt is generated at install time over the installed tree; see
# cmake/GenerateJsSha1Sum.cmake (declared last in the top-level CMakeLists).

if (USE_ENTERPRISE)
  # license-helper.js carries the TEST license signing keys (trusted only by
  # maintainer-mode binaries, see Enterprise/License/LicenseFeature.cpp) and
  # is required by enterprise license tests only — never ship it in releases.
  set(ARANGODB_JS_EE_TEST_EXCLUDES "")
  if (NOT USE_MAINTAINER_MODE)
    set(ARANGODB_JS_EE_TEST_EXCLUDES
      REGEX "^.*/js/client/modules/@arangodb/license-helper\\.js$"        EXCLUDE
      REGEX "^.*/js/common/modules/@arangodb/testutils"                   EXCLUDE
      REGEX "^.*/js/common/modules/@arangodb/graph/graphs-generation-enterprise\\.js$" EXCLUDE)
  endif ()
  install(
    DIRECTORY
      ${ARANGODB_SOURCE_DIR}/enterprise/js/common
      ${ARANGODB_SOURCE_DIR}/enterprise/js/client
    DESTINATION    ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}
    FILES_MATCHING PATTERN "*.js"
    ${ARANGODB_JS_EE_TEST_EXCLUDES}
  )
endif ()

# For the node modules we need all files except the following:
install(
  DIRECTORY ${ARANGODB_SOURCE_DIR}/js/node
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}
  REGEX "^.*/.bin"                                         EXCLUDE
  REGEX "^.*/.npmignore"                                   EXCLUDE
  REGEX "^.*/@sinonjs"                                     EXCLUDE
  REGEX "^.*/@xmldom"                                      EXCLUDE
  REGEX "^.*/ansi_up"                                      EXCLUDE
  REGEX "^.*/node-netstat"                                 EXCLUDE
  REGEX "^.*/parse-prometheus-text-format"                 EXCLUDE
  REGEX "^.*/sinon"                                        EXCLUDE
  REGEX "^.*/node/node_modules/is-wsl"                     EXCLUDE
  REGEX "^.*/node/node_modules/shallow-equal"              EXCLUDE
)
