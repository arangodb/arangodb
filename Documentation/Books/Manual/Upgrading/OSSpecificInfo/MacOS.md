Upgrading on MacOS
==================

This _Section_ describes upgrading an ArangoDB single-server installation, which
was installed via Homebrew or via the provided ArangoDB packages (*.dmg). 

Upgrading via Homebrew
--------------------------

First update the homebrew repository:

```
brew update
```

Then use **brew** to install the latest version of arangodb:

```
brew upgrade arangodb
```

Upgrading via Package
--------------------------

[Download](https://www.arangodb.com/download/) the latest ArangoDB Mac OS package and install it as usual by
mounting the `.dmg` file. Just drag and drop the `ArangoDB3-CLI` (community) or
the `ArangoDB3e-CLI` (enterprise) file into the shown `Applications` folder.
You will be asked if you want to replace the old file with the newer one.

![MacOSUpgrade](MacOSUpgrade.png) 

Select `Replace` to install the current ArangoDB version.

Upgrading more complex environments
--------------------------

The procedure described in this _Section_
is a first step to upgrade more complex deployments such as
[Cluster](../../Architecture/DeploymentModes/Cluster/README.md)
or [Active Failover](../../Architecture/DeploymentModes/ActiveFailover/README.md). 
