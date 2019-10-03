---
layout: default
description: ArangoDB Server Temp Options
---
# ArangoDB Server Temp Options

## Path

`temp.path`

Path for temporary files. ArangoDB will use this for storing temporary files, for
extracting data from uploaded zip files (e.g. for Foxx services) and other things.
Ideally the temporary path is set to an instance-specific subdirectory of the 
operating system's temporary directory.
To avoid data loss the temporary path should not overlap with any directories that 
contain important data, for example, the instance's database directory.

If the temporary path is set to the same directory as the instance's database directory,
a startup error will be logged from ArangoDB v3.4.8 onwards. ArangoDB v3.5 and higher will
additionally abort the startup with such configuration.
