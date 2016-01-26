

@brief renames a file
`fs.move(source, destination)`

Moves *source* to destination. Failure to move the file, or
specifying a directory for destination when source is a file will throw an
exception. Likewise, specifying a directory as source and destination will
fail.

