# ArangoDB Server Compaction Options (MMFiles)

The ArangoDB MMFiles storage engine will run a compaction over data files.

ArangoDB writes Documents in the WAL file. Once they have been sealed in the
WAL file, the collector may copy them into a per collection journal file.

Once journal files fill up, they're sealed to become data files.

One collection may have documents in the WAL logs, its journal file, and an
arbitrary number of data files.

If a collection is loaded, each of these files are opened (thus use a file
handle) and are mmap'ed. Since file handles and memory mapped files are also
a sparse resource, that number should be kept low.

Once you update or remove documents from data files (or already did while it was
the journal file) these documents are marked as 'dead' with a deletion marker.

Over time the number of dead documents may rise, and we don't want to use the
previously mentioned resources, plus the disk space should be given back to
the system. Thus several journal files can be combined to one, omitting the
dead documents.

Combining several of these data files into one is called compaction.
The compaction process reads the alive documents from the original data files,
and writes them into new data file.

Once that is done, the memory mappings to the old data files is released, and
the files are erased.

Since the compaction locks the collection, and also uses I/O resources, its
carefully configurable under which conditions the system should perform which
amount of these compaction jobs:

ArangoDB spawns one compactor thread per database. The settings below vary
in scope.

## Activity control

The activity control parameters alter the behavior in terms of scan / execution
frequency of the compaction.

Sleep interval between two compaction runs (in seconds):
`--compaction.db-sleep-time`

The number of seconds the collector thread will wait between two attempts to
search for compactable data files of collections in one Database.
If the compactor has actually executed work, a subsequent lookup is done.

Scope: Database.

Minimum sleep time between two compaction runs (in seconds):
`--compaction.min-interval`

When an actual compaction was executed for one collection, we wait for this
time before we execute the compaction on this collection again.
This is here to let eventually piled up user load be worked out.

Scope: collection.

## Source data files

These parameters control which data files are taken into account for a
compaction run. You can specify several criteria which each off may be
sufficient alone.

The scan over the data files belonging to one collection is executed from
oldest data file to newest; if files qualify for a compaction they may be
merged with newer files (containing younger documents).

Scope: Collection level, some are influenced by collection settings.

Minimal file size threshold original data files have to be below for
a compaction:
`--compaction.min-small-data-file-size`

This is the threshold which controls below which minimum total size a data file
will always be taken into account for the compaction.

Minimum unused count of documents in a datafile:
`--compaction.dead-documents-threshold`

Data files will often contain dead documents. This parameter specifies their
top most accetpeable count until the data file qualifies for compaction.

How many bytes of the source data file are allowed to be unused at most:
`--compaction.dead-size-threshold`

The dead data size varies along with the size of your documents.
If you have many big documents, this threshold may hit before the document
count threshold.

How many percent of the source data file should be unused at least:
`--compaction.dead-size-percent-threshold`

Since the size of the documents may vary this threshold works on the 
percentage of the dead documents size. Thus, if you have many huge
dead documents, this threshold kicks in earlier. 

To name an example with numbers, if the data file contains 800 kbytes of alive
and 400 kbytes of dead documents, the share of the dead documents is:

`400 / (400 + 800) = 33 %`.

If this value if higher than the specified threshold, the data file will
be compacted.

## Compacted target files

Once data files of a collection are qualified for a compaction run, these
parameters control how many data files are merged into one, (or even one source
data file may be compacted into one smaller target data file)

Scope: Collection level, some are influenced by collection settings.

Maximum number of files to merge to one file:
`--compaction.dest-max-files`

How many data files (at most) we may merge into one resulting data file during
one compaction run.

How large the resulting file may be in comparison to the collections `database.maximal-journal-size` setting:
`--compaction.dest-max-file-size-factor`

In ArangoDB you can configure a default *journal file size* globally and
override it on a per collection level. This value controls the size of
collected data files relative to the configured journal file size of the
collection in question.

A factor of 3 means that the maximum file size of the compacted file is
3 times the size of the maximum collection journal file size.

How large may the compaction result file become:
`--compaction.dest-max-result-file-size`

Next to the factor above, a totally maximum allowed file size in bytes may
be specified. This will overrule all previous parameters. 
