JavaScript Interface to Views
=============================

This is an introduction to ArangoDB's interface for views and how to handle
views from the JavaScript shell _arangosh_. For other languages see the
corresponding language API.

Address of a View
-----------------

Like [collections](../Collections/README.md), views are accessed by the user via
their unique name and internally via their identifier. Using the identifier for
accessing views is discouraged. Views share their namespace with collections,
so there cannot exist a view and a collection with the same name in the same
database.

Usage
-----

Here follow some basic usage examples. More details can be found in the
following chapters:

- [ArangoSearch Views](../../Views/ArangoSearch/README.md)
- [Database Methods for Views](DatabaseMethods.md)
- [View Methods](ViewMethods.md)

Create a view with default properties:

    @startDocuBlockInline viewUsage_01
    @EXAMPLE_ARANGOSH_OUTPUT{viewUsage_01}
    ~ db._create("colA");
    ~ db._create("colB");
    view = db._createView("myView", "arangosearch", {});
    ~ addIgnoreCollection("colA");
    ~ addIgnoreCollection("colB");
    ~ addIgnoreView("myView");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewUsage_01

Get this view again later by name:

    @startDocuBlockInline viewUsage_02
    @EXAMPLE_ARANGOSH_OUTPUT{viewUsage_02}
    view = db._view("myView");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewUsage_02

Get the view properties:

    @startDocuBlockInline viewUsage_03
    @EXAMPLE_ARANGOSH_OUTPUT{viewUsage_03}
    view.properties();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewUsage_03

Set a view property:

    @startDocuBlockInline viewUsage_04
    @EXAMPLE_ARANGOSH_OUTPUT{viewUsage_04}
    view.properties({cleanupIntervalStep: 12});
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewUsage_04

Add a link:

    @startDocuBlockInline viewUsage_05
    @EXAMPLE_ARANGOSH_OUTPUT{viewUsage_05}
    view.properties({links: {colA: {includeAllFields: true}}});
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewUsage_05

Add another link:

    @startDocuBlockInline viewUsage_06
    @EXAMPLE_ARANGOSH_OUTPUT{viewUsage_06}
    view.properties({links: {colB: {fields: {text: {}}}}});
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewUsage_06

Remove the first link again:

    @startDocuBlockInline viewUsage_07
    @EXAMPLE_ARANGOSH_OUTPUT{viewUsage_07}
    view.properties({links: {colA: null}});
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewUsage_07

Drop the view:

    @startDocuBlockInline viewUsage_08
    @EXAMPLE_ARANGOSH_OUTPUT{viewUsage_08}
    ~ removeIgnoreCollection("colA");
    ~ removeIgnoreCollection("colB");
    ~ removeIgnoreView("myView");
    db._dropView("myView");
    ~ db._drop("colA");
    ~ db._drop("colB");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewUsage_08
