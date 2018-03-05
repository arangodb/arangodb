Incompatible changes in ArangoDB 3.3
====================================

It is recommended to check the following list of incompatible changes **before**
upgrading to ArangoDB 3.3, and adjust any client programs if necessary.

The following incompatible changes have been made in ArangoDB 3.3:

* AQL: during a traversal if a vertex is not found, arangod will not log an error and 
  continue with a NULL value, but will instead register a warning at the query and 
  continue with a NULL value.

  If a non-existing vertex is referenced from a traversal, it is not desirable to log
  errors as ArangoDB can store edges pointing to non-existing vertices (which is perfectly 
  valid if the low-level insert APIs are used). As linking to non-existing vertices
  may indicate an issue in/with the data model or the client application, the warning is
  registered in the query so client applications have access to it.

* ArangoDB usernames must not start with the string `:role:`.

* The startup configuration parameter `--cluster.my-id` does not have any effect in 3.3.
  For compatibility reasons, ArangoDB 3.3 will not fail on startup if the option is 
  still used in the configuration, but it will silently ignore this option.

* The startup configuration parameter `--cluster.my-local-info` is deprecated now.
  Using it will make arangod log a warning on startup.

* Server startup: the recommended value for the Linux kernel setting in 
  `/proc/sys/vm/max_map_count` was increased to a value eight times as high as in 
  3.2. arangod compares at startup if the effective value of this setting is 
  presumably too low, and it will issue a warning in this case, recommending to 
  increase the value.
  
  This is now more likely to happen than in previous versions, as the recommended 
  value is now eight times higher than in 3.2. The startup warnings will look like
  this (with actual numbers varying):

      WARNING {memory} maximum number of memory mappings per process is 65530, which seems too low. it is recommended to set it to at least 512000

  Please refer to [the Linux kernel documentation](https://www.kernel.org/doc/Documentation/sysctl/vm.txt)
  for more information on this setting. This change only affects the Linux version of ArangoDB.


Client tools
------------

* The option `--recycle-ids` has been removed from the arangorestore command. 
  Using this option could have led to problems on the restore, with potential 
  id conflicts between the originating server (the source dump server) and the 
  target server (the restore server). 

* The option `--compat` has been removed from the arangodump command
  and the `/_api/replication/dump` REST API endpoint.
  In order to create a dump from an ArangoDB 2.8 instance, please use an older
  version of the client tools. Older ArangoDB versions are no longer be supported by 
  the arangodump and arangorestore binaries shipped with 3.3.

Miscellaneous
-------------

The minimum supported compiler for compiling ArangoDB from source is now g++ 5.4
(bumped up from g++ 4.9). This change only affects users that compile ArangoDB on
their own.
