

@brief returns the name for a (new) temporary file
`fs.getTempFile(directory, createFile)`

Returns the name for a new temporary file in directory *directory*.
If *createFile* is *true*, an empty file will be created so no other
process can create a file of the same name.

**Note**: The directory *directory* must exist.

