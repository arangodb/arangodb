# Writing BSON Sequences

When generating network packets you may come across the need to serialize multiple documents to BSON and then to a buffer.
To reduce the number of allocations and copies, Libbson provides the `bson_writer_t` interface.
It allows you to specify a memory location, size, and `realloc()` function to increase the allocation.

Using this structure allows you to avoid the intermediate step of serializing BSON to individual `bson_t` structures as well as `memcpy()` into the network buffer.
Instead we have a single power-of-two growing allocation which serves as both the network packet and the BSON serialization destination buffer.

## Writing a Series of BSON Documents

```c
bson_writer_t *writer;
uint8_t *buf = NULL;
size_t buflen = 0;
size_t offset = 20 + strlen("db.collection") + 1;
bson_t *doc;
int i;

/*
 * offset the destination by @offset bytes so that the OP_INSERT
 * packet can be placed before the serialization buffer.
 */
writer = bson_writer_new(&buf, &buflen, offset, bson_realloc);
for (i = 0; i < 100; i++) {
	bson_writer_begin(writer, &doc);
	bson_append_int32(doc, "hello", -1, i);
	bson_writer_end(writer);
}
bson_writer_destroy(writer);

/* Do something with buf of buflen bytes. */

bson_free(buf);
```
