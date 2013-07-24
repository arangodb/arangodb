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

import sys, os, json, string

files = { 
  "arangod/RestHandler/RestEdgeHandler.cpp" : "edge",
  "arangod/RestHandler/RestDocumentHandler.cpp" : "document",
  "arangod/RestHandler/RestReplicationHandler.cpp" : "replication",
  "js/actions/api-collection.js" : "collection",
  "js/actions/api-graph.js" : "graph",
  "js/actions/api-edges.js" : "edges",
  "js/actions/api-user.js" : "user",
  "js/actions/api-system.js" : "system",
  "js/actions/api-cursor.js" : "cursor",
  "js/actions/api-explain.js" : "explain",
  "js/actions/api-index.js" : "index",
  "js/actions/api-aqlfunction.js" : "aqlfunction",
  "js/actions/api-simple.js" : "simple",
  "js/actions/api-query.js" : "query",
  "js/actions/api-structure.js" : "structure",
  "js/actions/api-transaction.js" : "transaction",
  "js/actions/api-traversal.js" : "traversal"
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

apis = [ ];

# print "Generating Swagger docs for code in " + scriptDir + ", outdir: " + outDir + "\n"

for filename in files.keys():
  name = files[filename]
  outfile = outDir + name + ".json"

  os.system("python " + scriptDir + "Documentation/Scripts/generateSwagger.py < " + scriptDir + filename + " > " + outfile)
  apis.append({ "path" : relDir + name + ".{format}", "description" : name + " API" })


out = {
  "apiVersion" : "1.4",
  "swaggerVersion" : "1.1",
  "apis" : apis
}

print json.dumps(out, indent=4, separators=(', ',': '))

