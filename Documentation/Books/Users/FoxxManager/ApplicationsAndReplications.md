<a name="foxx_applications_and_replication"></a>
# Foxx Applications and Replication

Foxx applications consist of a file system part (scripts in the application directory)
and a database part. The current version of ArangoDB cannot replicate changes in the
file system so installing, updating or removing a Foxx application using `foxx-manager`
will not be included in the replication.
