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
./build-image_x86-64.sh
```

on an `x86_64` machine and then push the docker image produced. Then run

```
./build-image_arm64.sh
```

on an `arm64` machine, push the image, and then finally do

```
export IMAGE_TAG="..."
./build-manifest.sh
```

after setting the environment variable `IMAGE_TAG` to the output
of one of the previous scripts (without the architecture suffix!)
and follow the instructions on the screen to push the manifest.
