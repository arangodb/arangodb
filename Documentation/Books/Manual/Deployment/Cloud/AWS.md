Deploying ArangoDB on AWS
=========================

ArangoDB can be deployed on AWS or other cloud platforms. Deploying on a cloud
provider is common choice and many of the most big ArangoDB installation are running
on the cloud.

Up to ArangoDB 3.2, official ArangoDB AMI were available in the [AWS marketplace](https://aws.amazon.com/marketplace/search/results/ref=dtl_navgno_search_box?page=1&searchTerms=arangodb).
Such AMI are not being maintained anymore, though. However, deploying on AWS is
still possible, and again, a quite common scenario.

After having initialized your preferred AWS instance, with one of the ArangoDB supported
operating systems, using the [ArangoDB Starter](../ArangoDBStarter/README.md),
performing a [Manual Deployment](../Manually/README.md),
or using [Kubernetes](../Kubernetes/README.md), 
are all valid options to deploy on AWS. Please refer to the corresponding chapters for further 
information.

**Important:** In order to deploy on AWS, general guidelines, like using a fast,
**direct-attached**, SSD disk for the data directory of the ArangoDB processes
apply.
