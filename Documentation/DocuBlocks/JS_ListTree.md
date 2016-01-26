

@brief returns the directory tree
`fs.listTree(path)`

The function returns an array that starts with the given path, and all of
the paths relative to the given path, discovered by a depth first traversal
of every directory in any visited directory, reporting but not traversing
symbolic links to directories. The first path is always *""*, the path
relative to itself.

