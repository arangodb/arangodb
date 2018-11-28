Upgrading to ArangoDB 3.4
=========================

Please be sure that you have checked the list of
[incompatible changes in 3.4](../../ReleaseNotes/UpgradingChanges34.md)
before upgrading.

Upon upgrading from 3.3 to 3.4, the following storage engine-specific data conversion tasks
will be executed:

- **MMFiles storage engine:**

  All collection datafiles will be rewritten into a
  new data format. This data format is required in order to support using the collections
  in [ArangoSearch Views](../../Views/ArangoSearch/README.md) introduced in ArangoDB 3.4. 

  The conversion will read each datafile sequentially and write out a new datafile in the
  new format sequentially. This means the disk will be involved, but both reading and
  writing are done in a sequential fashion. Preliminary tests have shown that it will need 
  at most 2-3 times as long as it takes to copy the database directory.

- **RocksDB storage engine:**

  All existing geo indexes will be rewritten into a new 
  data format. This data format is required for using the indexes with the improved
  [geo index feature](../../Indexing/Geo.md) in ArangoDB 3.4. 
  
  Preliminary tests have shown that the conversion can process about 500K to 1M geo index 
  entries per second on commodity hardware.

If you upgrade without any existing data (new blank data folder), then none of these tasks
needs to be run because the datafiles will be created using the new format already.
