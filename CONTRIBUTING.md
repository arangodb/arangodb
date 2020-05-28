Contributing
============

We welcome bug fixes and patches from 3rd party contributors. Please
see the [Contributor Agreement](https://www.arangodb.com/community#contribute)
for details.

Please follow these guidelines if you want to contribute to ArangoDB:

Reporting Bugs
--------------

When reporting bugs, please use our issue tracker on GitHub.  Please make sure
to include the version number of ArangoDB in your bug report, along with the
platform you are using (e.g. `Linux OpenSuSE x86_64`).  Please also include the
ArangoDB startup mode (daemon, console, supervisor mode) plus any special
configuration.  This will help us reproducing and finding bugs.

Please also take the time to check there are no similar/identical issues open
yet.


Contributing features, documentation, tests
-------------------------------------------

* Create a new branch in your fork, based on the **devel** branch

* Develop and test your modifications there

* Commit as you like, but preferably in logical chunks. Use meaningful commit
  messages and make sure you do not commit unnecessary files (e.g. object
  files). It is normally a good idea to reference the issue number from the
  commit message so the issues will get updated automatically with comments.

* If the modifications change any documented behavior or add new features,
  document the changes. It should be written in American English.
  The documentation can be found at https://github.com/arangodb/docs

* When done, run the complete test suite and make sure all tests pass. You can
  check [README_maintainers.md](README_maintainers.md) for test run instructions.

* When finished, push the changes to your GitHub repository and send a pull
  request from your fork to the ArangoDB repository. Please make sure to select
  the appropriate branches there. This will most likely be **devel**.

* You must use the Apache License for your changes and have signed our 
  [CLA](https://www.arangodb.com/documents/cla.pdf). We cannot accept pull requests
  from contributors that didn't sign the CLA.

* Please let us know if you plan to work on a ticket. This way we can make sure
  redundant work is avoided.


Additional Resources
--------------------

* [ArangoDB website](https://www.arangodb.com/)

* [ArangoDB on Twitter](https://twitter.com/arangodb)

* [General GitHub documentation](https://help.github.com/)

* [GitHub pull request documentation](https://help.github.com/send-pull-requests/)
