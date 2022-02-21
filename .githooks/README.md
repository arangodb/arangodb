# Using commit hooks

We automatically check the C++ code that is pushed to our repository for correct 
formatting. There is a Github action running for all pushes that runs a code
formatting check that is based on clang-format.
This check will flag non-correctly formatted code as broken.

To make sure only correctly formatted code is pushed to Github, it is recommended
to install a Git pre-commit hook that checks every local commit for correct code
formatting.

In this directory, we provide such commit hooks for:
* [Linux](pre-commit-linux)
* [Windows](pre-commit-windows) - work in progress
* [macOS](pre-commit-macos) (symlink to pre-commit-linux)

They can be installed by copying the pre-commit script to `.git/hooks/pre-commit`.
Please note that there is also a seperate commit hook for the enterprise repository,
which should be copied to `enterprise/.git/hooks/pre-commit`.

The commit hook will automatically verify that all to-be-committed files are
correctly formatted, and will abort the commit process in case of a formatting
violation.

To apply correct formatting, it is possible to run a script that we provide:
* [clang-format-linux](../scripts/clang-format-linux.sh)
* [clang-format-windows](../scripts/clang-format-windows.sh) - work in progress
* [clang-format-macos](../scripts/clang-format-macos.sh) - (symlink to pre-commit-linux)

After running this script, the code should be correctly formatted. It is then
necessary to stage the formatting changes and commit again.
