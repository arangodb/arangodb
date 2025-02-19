# What is this?

This directory contains everything to build an Ubuntu based build image.
It is based on Ubuntu 23.10 and patches glibc with Then
  --enable-static-nss=yes
option. Then we can do our "normal" build image.

Furthermore, this image contains a pre-built version of v8 Version
12.1.165 under /opt/v8. This can be used to build devel based branches
which have the option to use an external v8 engine.

Simply run

```
make amd64
```

on an `x86_64` machine and then

```
make arm64
```

on an `arm64` machine and then finally do

```
make manifest
```

and follow the instructions on the screen to push the manifest.
