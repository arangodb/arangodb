Installing ArangoDB on Linux
============================

To install ArangoDB on Linux:

1. Visit the official [Download](https://www.arangodb.com/download) page of the
   ArangoDB web site and download the correct package for your Linux distribution.
   You can find binary packages for the most common distributions there. Linux Mint: 
   please use the corresponding Ubuntu or Debian packages. 
2. Follow the installation instructions on the _Download_ page to use your
   favorite package manager for the major distributions. After setting up the ArangoDB
   repository you can easily install ArangoDB using _yum_, _aptitude_, _urpmi_ or _zypper_.

{% hint 'info' %}
In addition to installation packages (distribution dependent) a `tar.gz` archive
is available starting from version 3.4.0.
{% endhint %}

After installation, you may start ArangoDB in several ways. The exact start-up command
depends on your Linux distribution, as well as on the type of ArangoDB deployment you
are interested in (_Single Server_, _Master-Slave_, _Active Failover_, _Cluster_, _DC2DC_).

Please refer to the [_Deployment_](../Deployment/README.md) chapter for details.

Securing your Installation
--------------------------

### Debian / Ubuntu

Debian based packages will ask for a password during installation. 

#### Securing Unattended Installations on Debian

For unattended installations, you can set the password using the
[debconf helpers](http://www.microhowto.info/howto/perform_an_unattended_installation_of_a_debian_package.html):

```
echo arangodb3 arangodb3/password password NEWPASSWORD | debconf-set-selections
echo arangodb3 arangodb3/password_again password NEWPASSWORD | debconf-set-selections
```

The commands above should be executed prior to the installation.

### Red-Hat / CentOS

Red-Hat based packages will set a random password during installation. The generated
random password is printed during the installation. Please write it down somewhere,
or change it to a password of your choice by executing:

```
ARANGODB_DEFAULT_ROOT_PASSWORD=NEWPASSWORD arango-secure-installation
```

The command should be executed after the installation.

### Other Distributions

For other distributions run `arango-secure-installation` to set a _root_ password.

{% hint 'danger' %}
Please be aware that running `arango-secure-installation` on your ArangoDB Server will remove
all current database users but root.
{% endhint %}
