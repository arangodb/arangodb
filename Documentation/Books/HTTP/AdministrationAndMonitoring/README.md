HTTP Interface for Administration and Monitoring
================================================

This is an introduction to ArangoDB's HTTP interface for administration and
monitoring of the server.

Logs
----

<!-- lib/Admin/RestAdminLogHandler.cpp -->

@startDocuBlock get_admin_log

@startDocuBlock get_admin_loglevel

@startDocuBlock put_admin_loglevel

Statistics
----------

<!-- js/actions/api-system.js -->

@startDocuBlock get_admin_statistics

<!-- js/actions/api-system.js -->

@startDocuBlock get_admin_statistics_description

Cluster
-------

<!-- js/actions/api-system.js -->

@startDocuBlock get_admin_server_mode

@startDocuBlock put_admin_server_mode

@startDocuBlock get_admin_server_id

@startDocuBlock get_admin_server_role

@startDocuBlock get_admin_server_availability

<!-- js/actions/api-cluster.js -->

@startDocuBlock get_cluster_statistics

@startDocuBlock get_cluster_health


Other
-----

<!-- js/actions/api-system.js -->

@startDocuBlock get_admin_routing_reloads
