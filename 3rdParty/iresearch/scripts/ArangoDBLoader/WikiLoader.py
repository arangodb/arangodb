# python script for loading IResearch benchmark dump of  Wikipedia into
# ArangoDB database. Uses python-arango driver https://github.com/Joowani/python-arango
# By default script loads 10M records into 'wikipedia' collection of '_system' database.
# Data is loaded in form { title: 'XXXXX', body: 'XXXXXXXXXXXXX'}.
# Collection should be already created. DB server should be set up to run without authorization and
# on localhost:8529

import csv
import ctypes
from arango import ArangoClient


def main():
  # Override csv default 128k field size
  csv.field_size_limit(int(ctypes.c_ulong(-1).value // 2))

  # Initialize the client for ArangoDB.
  client = ArangoClient(hosts='http://localhost:8529')
  
  # Upload settings
  filename = 'C:\\Working\\iresearch-toolkit\\iresearch\\iresearch.deps\\benchmark_resources\\benchmark.data' # data file
  collection = 'wikipedia' # target collection
  database = '_system'  # target database
  line_limit = 10000000 # how many documents to upload
  batch_size = 10000    # batch size for inserting into Arango

  db = client.db(database)
  wikipedia = db.collection(collection)
  f = open(filname, mode ='r', encoding='utf-8', errors='replace')
  reader = csv.reader(f, delimiter='\t')
  data = []
  total = 0
  for row in reader:
    data.append({'title': row[0], 'body': row[2]})
    if len(data) > batch_size:
      wikipedia.insert_many(data)
      data.clear()
    total = total + 1
    if total >= line_limit:
      break
  if len(data) > 0:
    wikipedia.insert_many(data)
  f.close()

  
if __name__== "__main__":
  main()