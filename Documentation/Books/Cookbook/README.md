![ArangoDB-Logo](https://docs.arangodb.com/assets/arangodb_logo_2016_inverted.png)

In this cookbook you can find some recipes for specific problems regarding ArangoDB.

The cookbook is always work in progress.

##How to contribute

Do you have recipes for the ArangoDB cookbook?  <br>
Write them down and we will add them!

It's pretty easy. You write a markdown file in the recipes folder. At the end
of your recipe you can add your name and some tags so other users can find your
recipe faster. The tags should be written in lower case:

**Author**: [yourName](https://github.com/yourName)

**Tags**: #arangodb #cookbook #aql

After that you edit `SUMMARY.md` and simply add a title and the file name to
the end of it. Then make a pull request.

For legal reasons, we require contributors to sign a contributor license
agreement (CLA). We are using a Apache 2 CLA for ArangoDB, which can be found
here: https://www.arangodb.com/documents/cla.pdf

Please fill out and sign the CLA, then scan and email it to cla (at) arangodb.com -
we will add your recipe as soon as possible after receiving it.

##How to build the cookbook locally

If you want to build and test the cookbook on your local computer, perform the
following steps (in a terminal / command line):

- Install [Node.js][1], if you don't have it already
- Install [Gitbook][2]:

  ```
  $ npm install gitbook -g
  ```

- Go to the `recipes` folder
- Run `gitbook install` to install the required Gitbook plugins
- Run `gitbook build` to generate the cookbook
- Open `recipes/_book/index.html` with your browser

[1]: https://nodejs.org/
[2]: https://github.com/GitbookIO/gitbook
