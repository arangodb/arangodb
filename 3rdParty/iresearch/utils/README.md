Put lucene-util downloaded data into the index:
```
./index-put -m put --in ../../lucene-tests/data/enwiki-20120502-lines-1k.txt --index-dir index.dir --max-lines 10000 --threads 1 --commit-period=10000
```

Run benchmark on the index:
```
./index-search -m search --in ../../lucene-tests/util/tasks/wikimedium.1M.nostopwords.tasks --index-dir index.dir --max-tasks 1 --repeat 20 --threads 2 --random
```

