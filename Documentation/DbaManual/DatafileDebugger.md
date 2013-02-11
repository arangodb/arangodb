Datafile Debugger {#DbaManualDatafileDebugger}
==============================================

@NAVIGATE_DbaManualDatafileDebugger
@EMBEDTOC{DbaManualDatafileDebuggerTOC}

In Case Of Disaster
===================

AranagoDB uses append-only journals. Data corruption should only occur when the
database server is killed. In this case, the corruption should only occur in the
last object(s) being written to the journal.

If a corruption occurs within a normal datafile, then this can only happen if a
hardware fault occurred.

If a journal or datafile is corrupt, shut down the database server and start
the program

    unix> arango-dfdb

in order to check the consistency of the datafiles and journals.
