# Install-time script: writes JS_SHA1SUM.txt into the installed JS directory
# (expects ARANGODB_INSTALLED_JS_DIR, set by an install(CODE) rule right
# before the install(SCRIPT) rule that runs this file).
#
# This replaces the checksum file that Installation/release.sh used to
# generate into the source tree: the checksum is
# now derived from exactly what was installed — including frontend and
# enterprise files — and nothing needs to be checked in.
#
# arangod (V8DealerFeature::copyInstallationFiles) compares the file content
# verbatim to decide whether to refresh its copied JS tree, so only content
# stability matters; the sha1sum-compatible line format is kept for
# familiarity and for manual verification.

if(NOT IS_DIRECTORY "${ARANGODB_INSTALLED_JS_DIR}")
  message(STATUS "No installed JS directory at ${ARANGODB_INSTALLED_JS_DIR}, skipping JS_SHA1SUM.txt")
  return()
endif()

file(REMOVE
  "${ARANGODB_INSTALLED_JS_DIR}/JS_FILES.txt"
  "${ARANGODB_INSTALLED_JS_DIR}/JS_SHA1SUM.txt")

file(GLOB_RECURSE _js_files LIST_DIRECTORIES false
  RELATIVE "${ARANGODB_INSTALLED_JS_DIR}"
  "${ARANGODB_INSTALLED_JS_DIR}/*")
list(SORT _js_files)

set(_js_lines "")
foreach(_js_file IN LISTS _js_files)
  file(SHA1 "${ARANGODB_INSTALLED_JS_DIR}/${_js_file}" _js_hash)
  string(APPEND _js_lines "${_js_hash}  ./${_js_file}\n")
endforeach()

# Equivalent of "sha1sum JS_FILES.txt" over the per-file list, without
# leaving JS_FILES.txt behind.
string(SHA1 _js_total "${_js_lines}")
file(WRITE "${ARANGODB_INSTALLED_JS_DIR}/JS_SHA1SUM.txt"
  "${_js_total}  JS_FILES.txt\n")
message(STATUS "Generated ${ARANGODB_INSTALLED_JS_DIR}/JS_SHA1SUM.txt (${_js_total})")
