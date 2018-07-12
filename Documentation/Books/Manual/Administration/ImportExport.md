Import and Export
=================

Import and export can be done via the tools [_arangoimport_](../Programs/Arangoimport/README.md) and [_arangoexport_](../Programs/Arangoexport/README.md)

<!-- Importing from files -->

<!-- Bulk import via HTTP API -->

<!-- Export to files -->

<!-- Bulk export via HTTP API -->

<!-- Syncing with 3rd party systems? -->

Converting JSON pretty printed files into JSONlines format
==========================================================

[_arangoimport_](../Programs/Arangoimport/README.md) may perform better with [_JSONlines_](http://jsonlines.org/) formatted input files.

Depending on the actual formatting of the JSON input file scaling issues may be ovecome using JSONlines.

You can easily use the `jq` command line tool for this conversion:

    jq -c '.[]' inputFile.json > outputFile.json 

