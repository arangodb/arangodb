!CHAPTER ArangoDB Shell Output

By default, the ArangoDB shell uses a pretty printer when JSON documents are
printed. This ensures documents are printed in a human-readable way:

    @startDocuBlockInline usingToArray
    @EXAMPLE_ARANGOSH_OUTPUT{usingToArray}
    db._create("five")
    for (i = 0; i < 5; i++) db.five.save({value:i})
    db.five.toArray()
    ~db._drop("five");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock usingToArray

While the pretty-printer produces nice looking results, it will need a lot of
screen space for each document. Sometimes, a more dense output might be better.
In this case, the pretty printer can be turned off using the command
*stop_pretty_print()*.

To turn on pretty printing again, use the *start_pretty_print()* command.

