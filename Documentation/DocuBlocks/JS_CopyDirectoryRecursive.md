

@brief copies a directory structure
`fs.copyRecursive(source, destination)`

Copies *source* to *destination*.
Exceptions will be thrown on:
 - Failure to copy the file
 - specifying a directory for destination when source is a file
 - specifying a directory as source and destination

