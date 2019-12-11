#!/usr/bin/python3

import pyresearch
import sys

index = pyresearch.index_reader.open(sys.argv[1])
segmentId = 0
terms = 0
for segment in index:
  #print (f"Segment [{segmentId}, {segment.docs_count()}]")
  for field in segment.fields():
    #print (f"Field {[field.name(), field.norm(), field.features(), field.min(), field.max(), field.docs_count()]}")
    termItr = field.iterator();
    while termItr.next():
      #print(f"  Term {termItr.value()}")
      if termItr.value() == "demographics".encode("utf-8"):
        print(f"  Term {termItr.value()} FOUND after looking {terms} through terms")
        exit() 
      terms = terms + 1
      #docs = termItr.postings()
      #print("       ",end="")
      #while docs.next():
      #  print (f"{docs.value()},", end="")
      #print()

  #for column in segment.columns():
    #print(f"Column {[column.name(), column.id()]}")

  #  columnValues = segment.column(column.id());
    #print("       ",end="")
  #  for key in columnValues:
      #print (f"{key},", end="")
  #  print()
print (f"Terms searched: {terms} and no luck :(")







