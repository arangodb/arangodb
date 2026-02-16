// How to regenerate: Take an empty cluster, create a new database "db"
// and dump the agency: This is the object under /arango/Plan/Collections/db
// The JSON has been reformatted with jq, then par and then spaces have
// been removed. The idea is to have relatively few lines and bytes.
<<<<<<< HEAD
{
  "s10070" : [
    "PRMR-ab4cdcec-bae6-4998-af25-a93c0b4a3ada",
    "PRMR-8cb161bd-aa92-4d02-a0c3-9e48096e18a0"
  ]
},
    "statusString" : "loaded",
                     "shardingStrategy" : "hash",
                                          "shardKeys" : ["_key"],
                                                        "replicationFactor" : 2,
                                                        "numberOfShards" : 1,
                                                        "keyOptions"
    : {"type" : "traditional", "allowUserKeys" : true},
      "isSystem" : true,
                   "name" : "_graphs",
                            "indexes" : [ {
                              "id" : "0",
                              "type" : "primary",
                              "name" : "primary",
                              "fields" : ["_key"],
                              "unique" : true,
                              "sparse" : false
                            } ],
                                        "isSmart" : false,
                                                    "id" : "10069",
                                                           "deleted"
    : false,
      "minReplicationFactor" : 1,
      "cacheEnabled" : false
})="
