---
layout: default
description: This is an introduction to ArangoDB's HTTP interface for administration andmonitoring of the server
---
HTTP Interface for Administration and Monitoring
================================================

This is an introduction to ArangoDB's HTTP interface for administration and
monitoring of the server.

Logs
----

<!-- lib/Admin/RestAdminLogHandler.cpp -->
{% docublock get_admin_log %}
{% docublock get_admin_loglevel %}
{% docublock put_admin_loglevel %}

Statistics
----------

<!-- js/actions/api-system.js -->
{% docublock get_admin_statistics %}

<!-- js/actions/api-system.js -->
{% docublock get_admin_statistics_description %}

Cluster
-------

<!-- js/actions/api-system.js -->
{% docublock get_admin_server_mode %}
{% docublock put_admin_server_mode %}
{% docublock get_admin_server_id %}
{% docublock get_admin_server_role %}
{% docublock get_admin_server_availability %}

<!-- js/actions/api-cluster.js -->
{% docublock get_cluster_statistics %}
{% docublock get_cluster_health %}


Other
-----

<!-- js/actions/api-system.js -->
{% docublock get_admin_routing_reloads %}
