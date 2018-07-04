Running ArangoDB on AWS
=======================

WIP

ArangoDB is available as AMI on the [AWS Marketplace][1].

(If you've already a running ArangoDB image on AWS and need an update, please have a look at [Updating ArangoDB on AWS](AwsUpdate.md)).

Here is a quick guide how to start:

* Go the [ArangoDB marketplace][2], select the latest version and click on **Continue**
* Use the **1-Click Launch** tab and select the size of the instance (**EC2 Instance Type**) you wish to use.
* Now you can continue with a click on **Accept Terms & Launch with 1-Click**.

**Note**: If you do not have a key pair a warning will appear after clicking and you will be asked to generate a key pair.

You successfully launched an ArangoDB instance on AWS.

![Launch ArangoDB](../assets/RunningOnAWS/launch.png)

The ArangoDB Web-Interface can be reached using the **Access Software** button or via public instance IP and the Port 8529 (e.g.: http://12.13.14.15:8529)
The default user is `root` and the password is the `Instance ID` (You can find the Instance ID on the instance list).

![Web Interface](../assets/RunningOnAWS/webInterface.png)

If you want to learn more about ArangoDB, start with the [ArangoDB First Steps][../../Manual/GettingStarted/index.html] in our Documentation or try one of our [Tutorials][5] or Cookbook recipes.

**Author**: [Ingo Friepoertner](https://github.com/ifcologne)

**Tags** : #aws, #amazon, #howto

[1]: https://aws.amazon.com
[2]: https://aws.amazon.com/marketplace/search/results/ref=dtl_navgno_search_box?page=1&searchTerms=arangodb
[3]: https://aws.amazon.com/marketplace/library
[5]: https://www.arangodb.com/tutorials/



Updating an ArangoDB Image on AWS
=================================

If you run an ArangoDB on AWS and used the provided [AMI in the AWS Marketplace](https://aws.amazon.com/marketplace/search/results/ref=dtl_navgno_search_box?page=1&searchTerms=arangodb), you at some point want to update to the latest release.
The process to submit and publish a new ArangoDB image to the marketplace takes some time and you might not find the latest release in the marketplace store yet. 

However, updating to the latest version is not that hard.

First, log in to the virtual machine with the user *ubuntu* and the public DNS name of the instance.

    ssh ubuntu@ec2-XX-XX-XXX-XX.us-west-2.compute.amazonaws.com
    

To start an update to a known version of ArangoDB you can use:

    sudo apt-get update
    sudo apt-get install arangodb=2.5.7
    

To upgrade an ArangoDB instance to a new major version (from 2.5.x to 2.6.x), use:

    sudo apt-get install arangodb
    

You might get a warning that the configuration file has changed:

    Configuration file '/etc/arangodb/arangod.conf'
    ==> Modified (by you or by a script) since installation.
    ==> Package distributor has shipped an updated version.
      What would you like to do about it ?  Your options are:
       Y or I  : install the package maintainer's version
       N or O  : keep your currently-installed version
         D     : show the differences between the versions
         Z     : start a shell to examine the situation
    The default action is to keep your current version.
    *** arangod.conf (Y/I/N/O/D/Z) [default=N] ?
    

You should stay with the current configuration (type "N"), as there are some changes made in the configuration for AWS. If you type "Y" you will lose access from your applications to the database so make sure that database directory and server endpoint are valid.

     --server.database-directory
        needs to be  `/vol/...` for AWS
     --server.endpoint
        needs to be `tcp://0.0.0.0:8529` for AWS
    

If you update to a new major version, you will be asked to `upgrade` so that a database migration can be started:

    sudo service arangodb upgrade
    sudo service arangodb start
    

Now ArangoDB should be back to normal.

For now we have to stick with this manual process but e might create a simpler update process in the future. Please provide feedback how you use our Amazon AMI and how we can improve your user experience.

**Author**: Ingo Friepoertner

**Tags:** #aws #upgrade

 [1]: https://aws.amazon.com/marketplace/search/results/ref=dtl_navgno_search_box?page=1&searchTerms=arangodb
