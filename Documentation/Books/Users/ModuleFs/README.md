<a name="module_"fs""></a>
# Module "fs"

<a name="file_system_module"></a>
### File System Module

The implementation follows the CommonJS specification 
[Filesystem/A/0](http://wiki.commonjs.org/wiki/Filesystem/A/0).

`fs.exists(path)`

Returns true if a file (of any type) or a directory exists at a given path. If the file is a broken symbolic link, returns false.

`fs.isDirectory(path)`

Returns true if the path points to a directory.

`fs.isFile(path)`

Returns true if the path points to a file.


`fs.list(path)`

The functions returns the names of all the files in a directory, in lexically sorted order. Throws an exception if the directory cannot be traversed (or path is not a directory).

Note: this means that list("x") of a directory containing "a" and "b" would return ["a", "b"], not ["x/a", "x/b"].

`fs.listTree(path)`

The function returns an array that starts with the given path, and all of the paths relative to the given path, discovered by a depth first traversal of every directory in any visited directory, reporting but not traversing symbolic links to directories. The first path is always "", the path relative to itself.


`fs.move(source, destination)`

Moves source to destination. Failure to move the file, or specifying a directory for target when source is a file will throw an exception.


`fs.read(filename)`

Reads in a file and returns the content as string. Please note that the file content must be encoded in UTF-8.


`fs.read64(filename)`

Reads in a file and returns the content as string. The file content is Base64 encoded.


`fs.remove(filename)`

Removes the file filename at the given path. Throws an exception if the path corresponds to anything that is not a file or a symbolic link. If "path" refers to a symbolic link, removes the symbolic link.

<!--
@anchor JSModuleFsExists
@copydetails JS_Exists

@CLEARPAGE
@anchor JSModuleFsIsDirectory
@copydetails JS_IsDirectory

@CLEARPAGE
@anchor JSModuleFsIsFile
@copydetails JS_IsFile

@CLEARPAGE
@anchor JSModuleFsList
@copydetails JS_List

@CLEARPAGE
@anchor JSModuleFsListTree
@copydetails JS_ListTree

@CLEARPAGE
@anchor JSModuleFsMove
@copydetails JS_Move

@CLEARPAGE
@anchor JSModuleFsRead
@copydetails JS_Read

@CLEARPAGE
@anchor JSModuleFsRead64
@copydetails JS_Read64

@CLEARPAGE
@anchor JSModuleFsRemove
@copydetails JS_Remove

@BNAVIGATE_JSModuleFs
-->