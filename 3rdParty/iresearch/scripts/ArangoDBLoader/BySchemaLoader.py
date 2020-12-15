# python script for loading some sample data into ArangoDB database. 
# Dependencies
#   * python-arango driver [https://github.com/Joowani/python-arango]
#   * mimesis [https://github.com/lk-geimfari/mimesis]
# DB server should be set up to run without authorization 

################################################################################
## DISCLAIMER
##
## Copyright 2020 ArangoDB GmbH, Cologne, Germany
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
##
## @author Andrey Abramov
################################################################################

from arango import ArangoClient
import importlib.util
import sys

def main():
  DEFAULT_BATCH_SIZE = 10000

  if len(sys.argv) < 6:
    print("Usage: host database collection schemaProvider count [batchSize={0}] Example: python BySchemaLoader.py 'http://localhost:8529' _system entities mySchemaProvider.py 10000000".format(DEFAULT_BATCH_SIZE))
    return

  # script setting
  host = sys.argv[1]
  database = sys.argv[2]   # target database
  collection = sys.argv[3] # target collection
  schemaProvider = sys.argv[4] # schema script
  count = int(sys.argv[5]) # number of JSONs to load
  batch_size = int(sys.argv[6]) if len(sys.argv) > 6 else DEFAULT_BATCH_SIZE

  # load JSON schema from schema script
  spec = importlib.util.spec_from_file_location("module.name", schemaProvider)
  mod = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(mod)
  schema = mod.getSchema()

  # Initialize the client for ArangoDB.
  client = ArangoClient(hosts=host)

  db = client.db(database)
  if db.has_collection(collection):
    collection = db.collection(collection)
  else:
    collection = db.create_collection(collection)

  loaded = 0
  while (loaded < count):
    toLoad = min(count, batch_size)
    docs = schema.create(iterations=toLoad)
    collection.insert_many(docs)
    loaded += toLoad
    print('Loaded ' + str(loaded) + ' ' + str(round((loaded/count) * 100, 2)) +  '% \n')

if __name__== "__main__":
  main()
