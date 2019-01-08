Known Issues in ArangoDB 3.3
============================

This page lists important issues affecting the 3.3.x versions of the ArangoDB suite of products.
It is not a list of all open issues.

Critical issues (ArangoDB Technical & Security Alerts) are also included at [https://www.arangodb.com/alerts/](https://www.arangodb.com/alerts/).

| # | Issue      |
|---|------------|
| 5 | **Date Added:** 2018-12-04 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** Parallel creation of collections using multiple client connections with the same database user may spuriously fail with "Could not update user due to conflict" warnings when setting user permissions on the new collections. A follow-up effect of this may be that access to the just-created collection is denied. <br> **Affected Versions:** 3.3.x (all) <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/arangodb#5342](https://github.com/arangodb/arangodb/issues/5342) |
| 4 | **Date Added:** 2018-11-30 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** Wrong suggestion printed in the log on how to optimize an OS setting, if followed, could cause ArangoDB to run into problems as the number of memory mappings will keep growing <br> **Affected Versions:** 3.3.0 to 3.3.19 <br> **Fixed in Versions:** 3.3.20 <br> **Reference:** [https://www.arangodb.com/alerts/tech03/](https://www.arangodb.com/alerts/tech03/)  |
| 3 | **Date Added:** 2018-11-16 <br> **Component:** Backup/Restore <br> **Deployment Mode:** All <br> **Description:** Users not included in the backup if _--server.authentication = true_ <br> **Affected Versions:** 3.3.0 to 3.3.13 <br> **Fixed in Versions:** 3.3.14 <br> **Reference:** [https://www.arangodb.com/alerts/tech02/](https://www.arangodb.com/alerts/tech02/) |
| 2 | **Date Added:** 2018-11-03 <br> **Component:** Security <br> **Deployment Mode:** All <br> **Description:** Unauthorized access to ArangoDB when using LDAP authentication <br> **Affected Versions:** 3.3.0 to 3.3.18 <br> **Fixed in Versions:** 3.3.19 <br> **Reference:** [https://www.arangodb.com/alerts/sec01/](https://www.arangodb.com/alerts/sec01/)  |
| 1 | **Date Added:** 2018-04-09 <br> **Component:** Storage <br> **Deployment Mode:** Single Instance <br> **Description:** Data corruption could happen under Linux <br> **Affected Versions:** 3.3.0 <br> **Fixed in Versions:** 3.3.1 <br> **Reference:** [https://www.arangodb.com/alerts/tech01/](https://www.arangodb.com/alerts/tech01/)  |
