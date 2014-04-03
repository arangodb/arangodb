# bson_oid_t

The `bson_oid_t` structure represents an `ObjectId` in MongoDB.
It is a 96-bit identifier that includes various information about the system generating the OID.

## Composition

 * 4 bytes : The UNIX timestamp in big-endian format.
 * 3 bytes : The first 3 bytes of `MD5(hostname)`.
 * 2 bytes : The `pid_t` of the current process. Alternatively the task-id if configured.
 * 3 bytes : A 24-bit monotonic counter incrementing from `rand()` in big-endian.

## Comparing

The typical way to sort in C is using `qsort()`.
Therefore, Libbson provides a `qsort()` compatible callback function named `bson_oid_compare()`.
It returns `less than 1`, `greater than 1`, or `0` depending on the equality of two `bson_oid_t` structures.

If you simply want to compare two `bson_oid_t` structures for equality, use `bson_oid_equal()`.

## Generating

To generate a `bson_oid_t`, you may use the following.
I suggest that you reuse the `bson_context_t` instead of creating one each time you generate a `bson_oid_t`.

```c
bson_context_t *ctx;
bson_oid_t oid;
char str[25];

ctx = bson_context_new(BSON_CONTEXT_THREAD_SAFE);
bson_oid_init(&oid, ctx);
bson_oid_to_string(&oid, str);
printf("Generated %s\n", str);
bson_context_destroy(ctx);
```

You can also parse a string contianing a `bson_oid_t`.
The input string <em>MUST</em> be 24 characters or more in length.

```c
bson_oid_t oid;

bson_oid_init_from_string(&oid, "123456789012345678901234");
```

If you need to parse may `bson_oid_t` in a tight loop and can guarantee the data is safe, you might consider using the inline variant.
It will be inlined into your code and reduce the need for a foreign function call.

```c
bson_oid_t oid;

bson_oid_init_from_string_unsafe(&oid, "123456789012345678901234");
```

## Hashing

If you need to store items in a hashtable, you may want to use the `bson_oid_t` as the key.
Libbson provides a hash function for just this purpose.
It is based on DJB hash.

```c
unsigned hash;

hash = bson_oid_hash(oid);
```

## Time

You can easily fetch the time that a `bson_oid_t` was generated using `bson_oid_get_time_t()`.

```c
time_t t;

t = bson_oid_get_time_t(oid);
printf("The OID was generated at %u\n", (unsigned)t);
```
