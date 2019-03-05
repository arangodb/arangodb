Deploying ArangoDB on Microsoft Azure
=====================================

ArangoDB can be deployed on Azure or other cloud platforms. Deploying on a cloud
provider is common choice and many of the most big ArangoDB installation are running
on the cloud.

No Azure-specific scripts or tools are needed to deploy on Azure. Deploying on Azure
is still possible, and again, a quite common scenario.

After having initialized your preferred Azure instance, with one of the ArangoDB supported
operating systems, using the [ArangoDB Starter](../ArangoDBStarter/README.md),
performing a [Manual Deployment](../Manually/README.md),
or using [Kubernetes](../Kubernetes/README.md)
are all valid options to deploy on Azure. Please refer to the corresponding chapters for further 
information.

**Important:** In order to deploy on Azure, general guidelines, like using a fast,
**direct-attached**, SSD disk for the data directory of the ArangoDB processes
apply.
