#!/usr/bin/python

# -*- coding: utf-8 -*-

################################################################################
### @brief creates a Swagger JSON api file
###
### @file
###
### DISCLAIMER
###
### Copyright by triAGENS GmbH - All rights reserved.
###
### The Programs (which include both the software and documentation)
### contain proprietary information of triAGENS GmbH; they are
### provided under a license agreement containing restrictions on use and
### disclosure and are also protected by copyright, patent and other
### intellectual and industrial property laws. Reverse engineering,
### disassembly or decompilation of the Programs, except to the extent
### required to obtain interoperability with other independently created
### software or as specified by law, is prohibited.
###
### The Programs are not intended for use in any nuclear, aviation, mass
### transit, medical, or other inherently dangerous applications. It shall
### be the licensee's responsibility to take all appropriate fail-safe,
### backup, redundancy, and other measures to ensure the safe use of such
### applications if the Programs are used for such purposes, and triAGENS
### GmbH disclaims liability for any damages caused by such use of
### the Programs.
###
### This software is the confidential and proprietary information of
### triAGENS GmbH. You shall not disclose such confidential and
### proprietary information and shall use it only in accordance with the
### terms of the license agreement you entered into with triAGENS GmbH.
###
### Copyright holder is triAGENS GmbH, Cologne, Germany
###
### @author Jan Steemann
### @author Copyright 2013, triAGENS GmbH, Cologne, Germany
################################################################################

import sys, os, os.path, json, string, operator

files = { 
  "aqlfunction" : [ "js/actions/api-aqlfunction.js" ],
  "batch" : [ "arangod/RestHandler/RestBatchHandler.cpp" ],
  "collection" : [ "js/actions/_api/collection/app.js" ],
  "cursor" : [ "arangod/RestHandler/RestCursorHandler.cpp" ],
  "database" : [ "js/actions/api-database.js" ],
  "document" : [ "arangod/RestHandler/RestDocumentHandler.cpp" ],
  "edge" : [ "arangod/RestHandler/RestEdgeHandler.cpp" ],
  "edges" : [ "js/actions/api-edges.js" ],
  "endpoint" : [ "js/actions/api-endpoint.js" ],
  "explain" : [ "js/actions/api-explain.js" ],
  "export" : [ "arangod/RestHandler/RestExportHandler.cpp" ],
  "graph" : [ "js/actions/api-graph.js" ],
  "import" : [ "arangod/RestHandler/RestImportHandler.cpp" ],
  "index" : [ "js/actions/api-index.js" ],
  "job" : [ "arangod/HttpServer/AsyncJobManager.h" ],
  "log" : [ "arangod/RestHandler/RestAdminLogHandler.cpp" ],
  "query" : [ "arangod/RestHandler/RestQueryHandler.cpp" ],
  "replication" : [ "arangod/RestHandler/RestReplicationHandler.cpp" ],
  "simple" : [ "js/actions/api-simple.js", "arangod/RestHandler/RestSimpleHandler.cpp" ],
  "structure" : [ "js/actions/api-structure.js" ],
  "system" : [ "js/actions/api-system.js" ],
  "tasks" : [ "js/actions/api-tasks.js" ],
  "transaction" : [ "js/actions/api-transaction.js" ],
  "traversal" : [ "js/actions/api-traversal.js" ],
  "user" : [ "js/actions/_api/user/app.js" ],
  "version" : [ "arangod/RestHandler/RestVersionHandler.cpp" ],
  "wal" : [ "js/actions/_admin/wal/app.js" ]
}

if len(sys.argv) < 3:
  print "usage: " + sys.argv[0] + " <scriptDir> <outDir> <relDir>"
  sys.exit(1)

scriptDir = sys.argv[1]
if not scriptDir.endswith("/"):
  scriptDir += "/"

outDir = sys.argv[2]
if not outDir.endswith("/"):
  outDir += "/"

relDir = sys.argv[3]
if not relDir.endswith("/"):
  relDir += "/"

# read ArangoDB version
f = open(scriptDir + "VERSION", "r")
for version in f:
  version = version.strip('\n')
f.close()


apis = [ ];

# print "Generating Swagger docs for code in " + scriptDir + ", outdir: " + outDir + "\n"

for name, filenames in sorted(files.items(), key=operator.itemgetter(0)):
  tmpname = scriptDir + "/arango-swagger-" + name
  if os.path.isfile(tmpname):
    os.remove(tmpname)

  with open(tmpname, 'w') as tmpfile:
    for tmp in filenames:
      with open(tmp) as infile:
        tmpfile.write(infile.read())   

  outfile = outDir + name + ".json"

  os.system("python " + scriptDir + "Documentation/Scripts/generateSwagger.py < " + tmpname + " > " + outfile)
  os.remove(tmpname)
  apis.append({ "path" : relDir + name + ".{format}", "description" : name + " API" })


out = {
  "apiVersion" : version,
  "swaggerVersion" : "1.1",
  "apis" : apis
}

print json.dumps(out, indent=4, separators=(', ',': '))

