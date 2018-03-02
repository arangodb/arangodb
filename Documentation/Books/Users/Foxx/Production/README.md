Running Foxx in production
==========================

This chapter will explain to you how to run Foxx in a production environment.
Be it a microservice, an API with an user interface or an internal library.

Before reading this chapter you should make sure to at least read one of the install sections beforehand to get a good staring point.

Production Mode
---------------

At first we will introduce the production mode and describe its side effects.
This is the default mode for all freshly installed Foxxes.
It will compute the routing of a Foxx only once and afterwards use the cached version.
This mode is not suitable for development, it has less error output and does not react to source file changes.

[Read More](Productionmode.md)

Debugging
---------

Next you will learn about the restricted debugging mechanisms.
The focus of this mode is to seal your source files and never send any stacktraces unintentionally.
However there might still be unknown bugs in your Foxx, these will still be logged into the server-side log.

[Read More](Debugging.md)

Upgrade to new versions
-----------------------

If you want to update to a new version of your application the next chapter is important for you:

[New Versions](Upgrade.md)
