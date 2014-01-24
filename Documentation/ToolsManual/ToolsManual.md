ArangoDB Client Tools (@VERSION) {#ToolsManual}
===============================================

@NAVIGATE_ToolsManual

This manual describes the client tools shipped with ArangoDB.

_arangosh_ is an interactive, JavaScript-enabled shell that can be used to administer
an ArangoDB server and run ad-hoc queries:

@CHAPTER_REF{UserManualArangosh}

_arangoimp_ is a tool to bulk insert data from JSON, CSV or tab-separated files (TSV)
files into an ArangoDB database:

@CHAPTER_REF{ImpManual}

ArangoDB since version 1.4 comes with two additional tools to support dumping data
from an ArangoDB database and reloading them. 

_arangodump_ is a tool to create logical backups of one or many collections of an 
ArangoDB database:

@CHAPTER_REF{DumpManual}

_arangorestore_ is a tool to reload data dumped with _arangodump_ into an
ArangoDB database. Data can be loaded into the same or a different database:

@CHAPTER_REF{RestoreManual}

Both _arangodump_ and _arangorestore_ are client tools that need to connect to a 
running ArangoDB server instance. _arangodump_ will write all its output into a 
directory that needs to be specified when invoking it. _arangorestore_ will read
files from a directory that was formerly created by invoking _arangodump_.

@BNAVIGATE_ToolsManual
