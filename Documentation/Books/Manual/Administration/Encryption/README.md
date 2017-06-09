# Encryption 

__This feature is only available in the Enterprise Edition.__

When you store sensitive data in your ArangoDB database, you want 
to protect that data under all circumstances. 
At runtime you will protect it with SSL transport encryption and strong authentication, 
but when the data is already on disk, you also need protection. 
That is where the Encryption feature comes in. 

The Encryption feature of ArangoDB will encrypt all data that ArangoDB 
is storing in your database before it is written to disk.

The data is encrypted with AES-256-CTR, which is a strong encryption
algorithm, that is very suitable for multi-processor environments. This means that 
your data is safe, but your database is still fast, even under load.

Most modern CPU's have builtin support for hardware AES encryption, which makes it even faster.

Note: The Encryption feature requires the RocksDB storage engine.

## Encryption keys 

The Encryption feature of ArangoDB requires a single 32-byte key per server.
It is recommended to use a different key for each server (when operating in a cluster configuration).
Make sure to protect these keys! 

That means:
- Do not write them to persistent disks or your server(s), always store them on an in-memory (`tmpfs`) filesystem.
- Transport your keys safely to your server(s). There are various tools for managing secrets like this (e.g. vaultproject.io).
- Store a copy of your key offline in a safe place. If you lose your key, there is NO way to get your data back.

## Configuration

To activate encryption of your database, pass the following option to `arangod`.

```
$ arangod \
    --rocksdb.encryption-keyfile=/mytmpfs/mySecretKey \
    --server.storage-engine=rocksdb
```

Note: You also have to activate the `rocksdb` storage engine.

Make sure to pass this option the very first time you start your database.
You cannot encrypt a database that already exists.

## Creating keys 

The encryption keyfile must contain 32 bytes of random data.

You can create it with a command line this.

```
dd if=/dev/random bs=1 count=32 of=yourSecretKeyFile
```

For security, it is best to create these keys offline (away from your database servers) and
directly store them in you secret management tool.
