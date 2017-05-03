Migrating 2.x services to 3.0
=============================

When migrating services from older versions of ArangoDB it is generally recommended you make sure they work in [legacy compatibility mode](../LegacyMode.md), which can also serve as a stop-gap solution.

This chapter outlines the major differences in the Foxx API between ArangoDB 2.8 and ArangoDB 3.0.

General changes
---------------

The `console` object in later versions of ArangoDB 2.x implemented a special Foxx console API and would optionally log messages to a collection. ArangoDB 3.0 restores the original behaviour where `console` is the same object available from the [console module](../../Appendix/JavaScriptModules/Console.md).
