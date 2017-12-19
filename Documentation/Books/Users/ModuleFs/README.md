Module "fs"
===========

The implementation tries to follow the CommonJS specification where possible.
[Filesystem/A/0](http://wiki.commonjs.org/wiki/Filesystem/A/0).

Single File Directory Manipulation
----------------------------------

#### exists
@startDocuBlock JS_Exists

#### isFile
@startDocuBlock JS_IsFile

#### isDirectory
@startDocuBlock JS_IsDirectory

#### size
@startDocuBlock JS_Size

#### mtime
@startDocuBlock JS_MTime

#### pathSeparator
`fs.pathSeparator`

If you want to combine two paths you can use fs.pathSeparator instead of */* or *\\*.

#### join
`fs.join(path, filename)`

The function returns the combination of the path and filename, e.g. fs.join(Hello/World, foo.bar) would return Hello/World/foo.bar.

#### getTempFile
@startDocuBlock JS_GetTempFile

#### getTempPath
@startDocuBlock JS_GetTempPath

#### makeAbsolute
@startDocuBlock JS_MakeAbsolute

#### chmod
@startDocuBlock JS_Chmod

#### list
@startDocuBlock JS_List

#### listTree
@startDocuBlock JS_ListTree

#### makeDirectory
@startDocuBlock JS_MakeDirectory

#### makeDirectoryRecursive
@startDocuBlock JS_MakeDirectoryRecursive

#### remove
@startDocuBlock JS_Remove

#### removeDirectory
@startDocuBlock JS_RemoveDirectory

#### removeDirectoryRecursive
@startDocuBlock JS_RemoveDirectoryRecursive

File IO
-------

#### read
@startDocuBlock JS_Read

#### read64
@startDocuBlock JS_Read64

#### readBuffer
@startDocuBlock JS_ReadBuffer

#### readFileSync
`fs.readFileSync(filename, encoding)`

Reads the contents of the file specified in `filename`. If `encoding` is specified,
the file contents will be returned as a string. Supported encodings are:
- `utf8` or `utf-8`
- `ascii`
- `base64`
- `ucs2` or `ucs-2`
- `utf16le` or `utf16be`
- `hex`

If no `encoding` is specified, the file contents will be returned in a Buffer
object.


#### save
@startDocuBlock JS_Save

#### writeFileSync
`fs.writeFileSync(filename, content)`

This is an alias for `fs.write(filename, content)`.

Recursive Manipulation
----------------------

#### copyRecursive
@startDocuBlock JS_CopyDirectoryRecursive

#### CopyFile
@startDocuBlock JS_CopyFile

#### move
@startDocuBlock JS_MoveFile

ZIP
---

#### unzipFile
@startDocuBlock JS_Unzip

#### zipFile
@startDocuBlock JS_Zip

