# Optional linker tests for the split arangoserver archives.
# Not needed for sibling subdir dependency analysis.
# Build explicitly, e.g.: make probe_actions
# Safe to delete this file when the probes are no longer useful.

function(add_arango_probe name lib)
  add_executable(probe_${name} EXCLUDE_FROM_ALL
    probe_main.cpp)

  target_link_options(probe_${name} PRIVATE
    "-Wl,--whole-archive"
    "$<TARGET_FILE:${lib}>"
    "-Wl,--no-whole-archive")

  target_link_libraries(probe_${name} PRIVATE ${lib})
endfunction()

add_arango_probe(actions        arango_actions)
add_arango_probe(auth           arango_auth)
add_arango_probe(cluster        arango_cluster)
add_arango_probe(feature_phases arango_feature_phases)
add_arango_probe(general_server arango_general_server)
add_arango_probe(rest_handler   arango_rest_handler)
add_arango_probe(rest_server    arango_rest_server)
add_arango_probe(sharding       arango_sharding)
add_arango_probe(statistics     arango_statistics)
add_arango_probe(transaction    arango_transaction)
add_arango_probe(vector_index   arango_vector_index)