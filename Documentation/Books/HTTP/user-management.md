---
layout: default
description: This is an introduction to ArangoDB's HTTP interface for managing users
---
HTTP Interface for User Management
==================================

This is an introduction to ArangoDB's HTTP interface for managing users.

The interface provides a simple means to add, update, and remove users.  All
users managed through this interface will be stored in the system collection
*_users*. You should never manipulate the *_users* collection directly.

This specialized interface intentionally does not provide all functionality that
is available in the regular document REST API.

Please note that user operations are not included in ArangoDB's replication.
{% docublock UserHandling_create %}
{% docublock UserHandling_grantDatabase %}
{% docublock UserHandling_grantCollection %}
{% docublock UserHandling_revokeDatabase %}
{% docublock UserHandling_revokeCollection %}
{% docublock UserHandling_fetchDatabaseList %}
{% docublock UserHandling_fetchDatabasePermission %}
{% docublock UserHandling_fetchCollectionPermission %}
{% docublock UserHandling_replace %}
{% docublock UserHandling_modify %}
{% docublock UserHandling_delete %}
{% docublock UserHandling_fetch %}
{% docublock UserHandling_fetchProperties %}
