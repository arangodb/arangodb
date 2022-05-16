# Boost.Build Configuration Examples

Examples of `user-config.jam` for convenience of contributors and
users who wish to run complete build of Boost.GIL tests and examples.

## Usage

Copy any of the provided user configuration files to `%HOME%\user-config.jam`
on Windows or `$HOME/user-config.jam` on Linux.

Refer to [Boost.Build User Manual](https://boostorg.github.io/build/)
for more details about use of configuration files.

## Examples

### `user-config-windows-vcpkg.jam`

For Windows, provides minimal configuration to consume GIL dependencies
installed using [vcpkg](https://github.com/Microsoft/vcpkg) in `C:\vcpkg`:

```console
C:\vcpkg --triplet x86-windows install libjpeg-turbo libpng tiff
C:\vcpkg --triplet x64-windows install libjpeg-turbo libpng tiff
```

The configuration recognises the two [vcpkg triplets](https://github.com/microsoft/vcpkg/blob/master/docs/users/triplets.md)
corresponding to Boost.Build `address-model` variants, `32` and `64`.


```console
C:\boost-root> b2.exe toolset=msvc-14.2 address-model=64 libs/gil/test/extension/io//simple
Performing configuration checks
...
    - libjpeg                  : yes
    - zlib                     : yes
    - libpng                   : yes
    - libtiff                  : yes
```

Similarly, use `address-model=32` to request 32-bit build target.
