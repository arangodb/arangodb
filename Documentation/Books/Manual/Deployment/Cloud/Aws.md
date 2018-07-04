Running ArangoDB on AWS
=======================

ArangoDB can be deployed on AWS or other cloud platforms. Deploying on a cloud
provider is common choice and many of the most big ArangoDB installation are running
on the cloud.

Up to ArangoDB 3.2, official ArangoDB AMI were available in the [AWS marketplace](https://aws.amazon.com/marketplace/search/results/ref=dtl_navgno_search_box?page=1&searchTerms=arangodb).
Such AMI is not being maintained anymore, though. However, deploying on AWS is
still possible, and again, a quite common scenario.

Using the [ArangoDB Starter](../ArangoDBStarter/README.md), [Kubernetes](../Kubernetes/README.md),
or performing a [Manual Deployment](../Manually/README.md) are all valid
options. Please refer to the corresponding chapter for further information.

**Important:** In order to deploy on AWS, general guidelines, like using a fast,
local, SSD disk for the data directory of the ArangoDB processes apply.
