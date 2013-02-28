# Contributing

We welcome bug fixes and patches from 3rd party contributors.
Please follow these guidelines if you want to contribute to ArangoDB:

## Getting started

* Please make sure you have a GitHub account
* Please look into the ArangoDB issue tracker on GitHub for similar/identical issues
* For bugs: if the bug you found is not yet described in an existing issue, please file a new one. The new issue should include a clear description of the bug and how to reproduce it (including your environment)
* For feature requests: please clearly describe the proposed feature, additional configuration options, and side effects
* Please let us know if you plan to work an a ticket. This way we can make sure we avoid redundant work

* Create a fork our repository. You can use GitHub to do this
* Clone the fork to your development box and pull the latest changes from the ArangoDB repository. Please make sure to use the appropriate branch:
  * the "devel" branch is normally used for new features 
  * bug fixes should be done in the "devel" first, before being applied to master or other branches
* If missing, install the required prerequisites. They are listed [here](https://github.com/triAGENS/ArangoDB/wiki/Compiling).
* configure and make your local clone. If you intend to modify the parser files, please make sure to activate the --enable-maintainer-mode configure option. In this case, you also need to have Python installed.
* Make sure the unmodified clone works locally before making any code changes. You can do so by running the included test suite (i.e. make unittests)
* If you intend to do documentation changes, you also must install Doxygen in the most recent version.

## Making Changes

* Create a new branch in your fork
* Develop and test your modifications there
* Commit as you like, but preferrably in logical chunks. Use meaningful commit messages and make sure you do not commit unneccesary files (e.g. object files). It is normally a good idea to reference the issue number from the commit message so the issues will get updated automatically with comments
* If the modifications change any documented behavior or add new features, document the changes. The documentation can be found in arangod/Documentation directory. To recreate the documentation locally, run make doxygen. This will re-create all documentation files in the Doxygen directory in your repository. You can inspect the documentation in this folder using a text editor or a browser
* When done, run the complete test suite and make sure all tests pass
* When finished, push the changes to your GitHub repository and send a pull request from your fork to the ArangoDB repository. Please make sure to select the appropriate branches there
* You must use the Apache License for your changes

## Reporting Bugs

When reporting bugs, please use our issue tracker on GitHub.
Please make sure to include to include the version number of ArangoDB in your bug report, along with the platform you are using (e.g. `Linux OpenSuSE x86_64`). 
Please also include the ArangoDB startup mode (daemon, console, supervisor mode) plus any special configuration.
This will help us reproducing and finding bugs.

## Additional Resources

* [ArangoDB website](https://www.arangodb.org/)
* [ArangoDB on Twitter](https://twitter.com/arangodb)
* [General GitHub documentation](https://help.github.com/)
* [GitHub pull request documentation](https://help.github.com/send-pull-requests/)
