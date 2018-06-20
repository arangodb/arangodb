Upgrading on Linux
==================

By installing the new ArangoDB package the standalone instance is automatically upgraded. In the process the new ArangoDB version binary, which also includes the latest ArangoDB Starter binary, is deployed.

This procedure is a first step to upgrade complex deployments such as [Cluster](../../Scalability/Cluster/README.md) or [Active Failover](../../Scalability/ActiveFailover/README.md). 

Upgrading via APT (Ubuntu)
--------------------------

First add the repository key to apt like this:
```
curl -OL https://download.arangodb.com/arangodb33/xUbuntu_17.04/Release.key
sudo apt-key add - < Release.key
```

Use **apt-get** to install arangodb:
```
echo 'deb https://download.arangodb.com/arangodb33/xUbuntu_17.04/ /' | sudo tee /etc/apt/sources.list.d/arangodb.list
sudo apt-get install apt-transport-https
sudo apt-get update
sudo apt-get install arangodb3=3.3.10
```

**Note**: The latest available version can be found in the [download section](https://www.arangodb.com/download-major/ubuntu/).

Upgrading via DPKG (Ubuntu)
---------------------------
Download the corresponding file from the [download section](https://download.arangodb.com/).

Install a specific package using **dpkg**:

```
$ dpkg -i arangodb3-3.3.10-1_amd64.deb
```

Upgrading via YUM (CentOS)
-------------------------

Use **yum** to install arangodb:
```
cd /etc/yum.repos.d/
curl -OL https://download.arangodb.com/arangodb33/CentOS_7/arangodb.repo
yum -y install arangodb3-3.3.10
```
**Note**: The latest available version can be found in the [download section](https://www.arangodb.com/download-major/centos/).

Upgrading via RPM (CentOS)
---------------------------
Download the corresponding file from the [download section](https://download.arangodb.com/).

Install a specific package using **rpm**:

```
$ rpm -i arangodb3-3.3.10-1.x86_64.rpm
```
