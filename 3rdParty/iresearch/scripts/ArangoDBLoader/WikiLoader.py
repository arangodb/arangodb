# python script for loading IResearch benchmark dump of  Wikipedia into
# ArangoDB database. Uses python-arango driver https://github.com/Joowani/python-arango
# Data is loaded in form { title: 'XXXXX', body: 'XXXXXXXXXXXXX', 'count': XXXX, 'created':XXXX}.
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
## @author Andrei Lobov
################################################################################

import sys
import csv
import ctypes
from arango import ArangoClient

monthDecode = {
  "JAN":"01", "FEB":"02", "MAR":"03", "APR":"04",
  "MAY":"05", "JUN":"06", "JUL":"07", "AUG":"08",
  "SEP":"09", "OCT":"10", "NOV":"11", "DEC":"12"
} 

def decodeDate(d):
 if len(d) == 24:
   month = d[3:6]
   day = d[0:2]
   year = d[7:11]
   time = d[12:24]
   year += "-";
   year += monthDecode.get(month, "01")
   year += "-"
   year += day
   year += "T"
   year += time
   return year
 return d

def main():
  if len(sys.argv) < 6:
    print("Usage: host database collection data_file count [offset] Example: python WikiLoader.py 'http://localhost:8529' _system wikipedia benchmark.data 10000000")
    return


  # Override csv default 128k field size
  csv.field_size_limit(int(ctypes.c_ulong(-1).value // 2))

  # Initialize the client for ArangoDB.
  client = ArangoClient(hosts=sys.argv[1])
  
  # Upload settings
  filename = sys.argv[4] # data file
  collection = sys.argv[3] # target collection
  database = sys.argv[2]  # target database
  line_limit = int(sys.argv[5]) # how many documents to upload
  batch_size = 10000    # batch size for inserting into Arango
  
  offset = 0
  if len(sys.argv) > 6:
    offset = int(sys.argv[6])

  db = client.db(database)
  if db.has_collection(collection):
    wikipedia = db.collection(collection)
  else:
    wikipedia = db.create_collection(collection)
  f = open(filename, mode ='r', encoding='utf-8', errors='replace')
  reader = csv.reader(f, delimiter='\t')
  data = []
  total = 0
  count = offset
  for row in reader:
    if offset > 0:
      offset = offset - 1
      continue
    data.append({'title': row[0], 'body': row[2], 'count': count, 'created':decodeDate(row[1])})
    if len(data) > batch_size:
      wikipedia.insert_many(data)
      data.clear()
      print('Loaded ' + str(total) + ' ' + str( round((total/line_limit) * 100, 2)) +  '% \n')
    total = total + 1
    if total >= line_limit:
      break
    count = count + 1
  if len(data) > 0:
    wikipedia.insert_many(data)
    print('Loaded ' + str(total) + ' ' + str( round((total/line_limit) * 100, 2)) +  '% \n')
  f.close()

  
if __name__== "__main__":
  main()
