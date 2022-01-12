# CMake Configuration Examples

Examples of configuration files for CMake integrations in popular IDEs are provided
for convenience of users and contributors who wish to build, run and debug
Boost.GIL tests and examples in the IDEs of their choice.

## Visual Studio

Example [CMakeSettings.json](CMakeSettings.json) file is provided for
the [CMake support in Visual Studio](https://go.microsoft.com//fwlink//?linkid=834763).

Currently, the `CMakeSettings.json` provides configurations for the following
CMake generators:
- Ninja (default)
- Visual Studio 2017 and 2019
- `Unix Makefiles` targeting Windows Subsystem for Linux (WSL) - requires Visual Studio 2019 IDE.

Usage:

1. Copy [CMakeSettings.json](CMakeSettings.json) to `${BOOST_ROOT}/libs/gil`.
2. In Visual Studio > File > Open > Folder... and select `${BOOST_ROOT}/libs/gil`.
3. Follow the [CMake support in Visual Studio](https://go.microsoft.com//fwlink//?linkid=834763) documentation.
4. [CMakeSettings.json schema reference](https://docs.microsoft.com/en-us/cpp/build/cmakesettings-reference?view=vs-2017)
   to learn more about the configuration file itself.

Optionally, edit [CMakeSettings.json](CMakeSettings.json) and tweak any options you require.

## Visual Studio Code

Example of [cmake-variants.yaml](cmake-variants.yaml) file is provided for
the [CMake Tools](https://github.com/vector-of-bool/vscode-cmake-tools) extension.

Usage:

1. Copy [cmake-variants.yaml](cmake-variants.yaml) to `${BOOST_ROOT}/libs/gil`.
2. Run `code ${BOOST_ROOT}/libs/gil` and the set of variants will be loaded.
3. Follow the [CMake Tools documentation](https://vector-of-bool.github.io/docs/vscode-cmake-tools/index.html).

Optionally, edit [cmake-variants.yaml](cmake-variants.yaml)and tweak any options you require.
