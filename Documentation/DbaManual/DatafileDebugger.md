Datafile Debugger {#DbaManualDatafileDebugger}
==============================================

@NAVIGATE_DbaManualDatafileDebugger
@EMBEDTOC{DbaManualDatafileDebuggerTOC}

In Case Of Disaster
===================

AranagoDB uses append-only journals. Data corruption should only occur when the
database server is killed. In this case, the corruption should only occur in the
last object(s) that have being written to the journal.

If a corruption occurs within a normal datafile, then this can only happen if a
hardware fault occurred.

If a journal or datafile is corrupt, shut down the database server and start
the program

    unix> arango-dfdb

in order to check the consistency of the datafiles and journals. This brings up

	___      _         __ _ _           ___  ___    ___ 
       /   \__ _| |_ __ _ / _(_) | ___     /   \/ __\  / _ \
      / /\ / _` | __/ _` | |_| | |/ _ \   / /\ /__\// / /_\/
     / /_// (_| | || (_| |  _| | |  __/  / /_// \/  \/ /_\\ 
    /___,' \__,_|\__\__,_|_| |_|_|\___| /___,'\_____/\____/ 

    Available collections:
      0: _structures
      1: _users
      2: _routing
      3: _modules
      4: _graphs
      5: products
      6: prices
      *: all

    Collection to check: 

You can now select, which collection you want to check. After you selected one
or all collections, a consistency check is performed.

    Checking collection #1: _users

    Database
      path: /usr/local/var/lib/arangodb

    Collection
      name: _users
      identifier: 82343

    Datafiles
      # of journals: 1
      # of compactors: 1
      # of datafiles: 0

    Datafile
      path: /usr/local/var/lib/arangodb/collection-82343/journal-1065383.db
      type: journal
      current size: 33554432
      maximal size: 33554432
      total used: 256
      # of entries: 3
      status: OK

If there is a problem with one of the datafile, then the database debugger tries
to fixed that problem.

    WARNING: The journal was not closed properly, the last entries is corrupted.
             This might happen ArangoDB was killed and the last entries were not
             fully written to disk.

    Wipe the last entries (Y/N)?

If you answer `Y`, the corrupted entry will be removed.

If you see a corruption in a datafile (and not a journal), then something is
terrible wrong. These files are immutable and never changed by ArangoDB. A
corruption in such a file is an indication of a hard-disk failure.
