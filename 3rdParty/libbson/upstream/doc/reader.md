# Reading BSON Sequences

Libbson contains `bson_reader_t` to ease the task of parsing a sequence of BSON documents.
You may have such a case when reading many documents from disk or from an incoming MongoDB network packet.

## Iterating Documents with bson_reader_t

The following example shows how to read a series of documents and then print them as JSON.

```c
bson_reader_t reader;
const bson_t *b;
bson_bool_t eof = FALSE;
int fd;

fd = open(filename, O_RDONLY);
if (fd == -1) {
	perror("Failed to open file.");
	exit(1);
}

/* Initialize reader to read from fd and close() when we are done */
bson_reader_init_from_fd(&reader, fd, TRUE);
while ((b = bson_reader_read(&reader, &eof))) {
	str = bson_as_json(b, NULL);
	printf("%s\n", str);
	bson_free(str);
}
bson_reader_destroy(&reader);
```
