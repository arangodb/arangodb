<a name="administrating_arangodb"></a>
# Administrating ArangoDB

<a name="mostly_memory/durability"></a>
### Mostly Memory/Durability

Database documents are stored in memory-mapped files. Per default, these
memory-mapped files are synced regularly but not instantly. This is often a good
tradeoff between storage performance and durability. If this level of durability
is too low for an application, the server can also sync all modifications to
disk instantly. This will give full durability but will come with a performance
penalty as each data modification will trigger a sync I/O operation.

<a name="appendonly/mvcc"></a>
## AppendOnly/MVCC


Instead of overwriting existing documents, a completely new version of the
document is generated. The two benefits are:

- Objects can be stored coherently and compactly in the main memory.
- Objects are preserved-isolated writing and reading transactions allow
  accessing these objects for parallel operations.

The system collects obsolete versions as garbage, recognizing them as
forsaken. Garbage collection is asynchronous and runs parallel to other
processes.

<a name="configuration"></a>
## Configuration

<a name="global_configuration"></a>
### Global Configuration

There are certain default values, which you can store in the configuration file
or supply on the command line.

@copydetails triagens::arango::ArangoServer::_defaultMaximalSize

<a name="per_collection_configuration"></a>
### Per Collection Configuration

You can configure the durability behavior on a per collection basis.
Use the ArangoDB shell to change these properties.

@copydetails JS_PropertiesVocbaseCol

