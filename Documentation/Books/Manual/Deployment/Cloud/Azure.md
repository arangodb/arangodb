Deploying ArangoDB on Microsoft Azure
=====================================

ArangoDB can be deployed on Azure or other cloud platforms. Deploying on a cloud
provider is common choice and many of the most big ArangoDB installation are running
on the cloud.

No specific scripts or tools are available to deploy on Azure. Deploying on Azure
is still possible, and again, a quite common scenario.

Using the [ArangoDB Starter](../ArangoDBStarter/README.md), [Kubernetes](../Kubernetes/README.md),
or performing a [Manual Deployment](../Manually/README.md) are all valid
options to deploy on Azure. Please refer to the corresponding chapters for further 
information.

**Important:** In order to deploy on Azure, general guidelines, like using a fast,
local, SSD disk for the data directory of the ArangoDB processes apply.
