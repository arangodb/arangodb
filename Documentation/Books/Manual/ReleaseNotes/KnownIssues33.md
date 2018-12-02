Known Issues
============

This page lists some known issues of the ArangoDB suite of products. 

ArangoDB Customers can access the [_Technical & Security Alerts_](https://arangodb.atlassian.net/wiki/spaces/DEVSUP/pages/223903745) page after logging in the Support Portal.

**test**

| # | Issue      |
|---|------------|
| 4 | **Date Added:** 2018-11-30 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** Wrong suggestion printed in the log on how to optimize an OS setting, if followed, could cause ArangoDB to run into problems as the number of memory mappings will keep growing <br> **Affect Versions:** 3.3.0 to 3.3.19 <br> **Fixed in Versions:** 3.3.20 <br> **Reference:** [Link for ArangoDB Customers](https://arangodb.atlassian.net/plugins/servlet/servicedesk/customer/confluence/shim/spaces/DEVSUP/pages/228622337/Technical+Alert+%232%3A+Set+Linux+variable+overcommit_memory+to+0+or+1)  |
| 3 | **Date Added:** 2018-11-16 <br> **Component:** Security <br> **Deployment Mode:** All <br> **Description:** Unauthorized access to ArangoDB when using LDAP authentication <br> **Affect Versions:** 3.3.0 to 3.3.18 <br> **Fixed in Versions:** 3.3.19 <br> **Reference:** [Link for ArangoDB Customers](https://arangodb.atlassian.net/servicedesk/customer/kb/view/223903752)  |
| 2 | **Date Added:** 2018-11-03 <br> **Component:** Backup/Restore <br> **Deployment Mode:** All <br> **Description:** Users not included in the backup if _--server.authentication = true_ <br> **Affect Versions:** 3.3.0 to 3.3.13 <br> **Fixed in Versions:** 3.3.14 <br> **Reference:** [Link for ArangoDB Customers](https://arangodb.atlassian.net/servicedesk/customer/kb/view/226557953) |
| 1 | **Date Added:** 2018-04-09 <br> **Component:** Storage <br> **Deployment Mode:** Single Instance <br> **Description:** Data corruption could happen under Linux <br> **Affect Versions:** 3.3.0 <br> **Fixed in Versions:** 3.3.1 <br> **Reference:** [Link for ArangoDB Customers](https://arangodb.atlassian.net/plugins/servlet/servicedesk/customer/confluence/shim/spaces/DEVSUP/pages/164069377/Important+Note+for+Users+running+ArangoDB+v.+3.2.4%2C+3.2.5%2C+3.2.6%2C+3.2.7%2C+3.2.8%2C+3.2.9+or+3.3.0+on+Linux)  |

**end test**


| # | Date Added | Component | Deployment Mode | Description | Affect Versions | Fixed in Versions |  Reference |
|---|------------|:---------:|:---------------:|-------------|-----------------|-------------------|------------|
| 4 | 2018-11-30 | arangod | All | Wrong suggestion printed in the log on how to optimize an OS setting, if followed, could cause ArangoDB to run into problems as the number of memory mappings will keep growing | 3.3.0 to 3.3.19 | 3.3.20 | [Link for ArangoDB Customers](https://arangodb.atlassian.net/plugins/servlet/servicedesk/customer/confluence/shim/spaces/DEVSUP/pages/228622337/Technical+Alert+%232%3A+Set+Linux+variable+overcommit_memory+to+0+or+1) |
| 3 | 2018-11-16 | Security | All | Unauthorized access to ArangoDB when using LDAP authentication | 3.3.0 to 3.3.18 | 3.3.19 | [Link for ArangoDB Customers](https://arangodb.atlassian.net/servicedesk/customer/kb/view/223903752) |
| 2 | 2018-11-03 | Backup/Restore | All | Users not included in the backup if _--server.authentication = true_ | 3.3.0 to 3.3.13 | 3.3.14 | [Link for ArangoDB Customers](https://arangodb.atlassian.net/servicedesk/customer/kb/view/226557953) |
| 1 | 2018-04-09 | Storage | Single Instance | Data corruption could happen under Linux	 | 3.3.0 | 3.3.1 | [Link for ArangoDB Customers](https://arangodb.atlassian.net/plugins/servlet/servicedesk/customer/confluence/shim/spaces/DEVSUP/pages/164069377/Important+Note+for+Users+running+ArangoDB+v.+3.2.4%2C+3.2.5%2C+3.2.6%2C+3.2.7%2C+3.2.8%2C+3.2.9+or+3.3.0+on+Linux) |
