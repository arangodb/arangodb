

@brief returns the directory listing
`fs.list(path)`

The functions returns the names of all the files in a directory, in
lexically sorted order. Throws an exception if the directory cannot be
traversed (or path is not a directory).

**Note**: this means that list("x") of a directory containing "a" and "b" would
return ["a", "b"], not ["x/a", "x/b"].

