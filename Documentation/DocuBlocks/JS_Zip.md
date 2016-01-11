

@brief zips a file
`fs.zipFile(filename, chdir, files, password)`

Stores the files specified by *files* in the zip file *filename*. If
the file *filename* already exists, an error is thrown. The list of input
files *files* must be given as a list of absolute filenames. If *chdir* is
not empty, the *chdir* prefix will be stripped from the filename in the
zip file, so when it is unzipped filenames will be relative.
Specifying a password is optional.

Returns *true* if the file was zipped successfully.

