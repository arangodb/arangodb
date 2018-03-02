Develop your own Foxx
=====================

This chapter will explain to you how to write a Foxx on your own and use it to enhance the ArangoDB's functionality.
Be it a microservice, an API with a user interface or an internal library.

Before reading this chapter you should make sure to at least read one of the install sections beforehand to get a good staring point.
Recommended is the [generate](../Install/Generate.md) section to get started with your own Foxx, but you can also start with an existing one.

Development Mode
----------------

At first we will introduce the development mode and describe its side effects.
You can skip this section if you do not have access to the file system of ArangoDB as you will not get the benefits of this mode.
You will have to stick to the procedure described in [New Versions](../Production/Upgrade.md).

[Read More](Developmentmode.md)

Debugging
---------

Next you will learn about the debugging mechanisms if you have set a Foxx into development mode.
The app will return more debugging information in this mode like stacktraces.
In production mode stacktraces will be kept internally.

[Read More](Debugging.md)

Folder structure
----------------

If you want to get started with coding this is the section to begin with.
It will introduce the folder structure and describes where which files have to be located on server side.

[Read More](Folder.md)

Framework and tools
-------------------

Now we are entering the reference documentation of tools available in the Foxx framework.
The tools contain:

  * [Controller](../Develop/Controller.md)
  * [Setup & Teardown](../Develop/Scripts.md)
  * [Repository](../Develop/Repository.md)
  * [Model](../Develop/Model.md)
  * [Queries](../Develop/Queries.md)
  * [Background Tasks](../Develop/Queues.md)
  * [Console API](../Develop/Console.md)
  * you may use [Javascript Modules](../../ModuleJavaScript/README.md) and [install node modules using npm](../../ModuleJavaScript/README.md#installing-npm-modules) in the directory containing your `manifest.json`.

Finally we want to apply some meta information to the Foxx.
How this is done is described in the [Meta information](../Develop/Manifest.md) chapter.

