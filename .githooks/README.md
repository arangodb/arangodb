# Using commit hooks

We automatically check the C++ code that is pushed to our repository for correct 
formatting. There is a Github action running for all pushes that runs a code
formatting check that is based on clang-format.
This check will flag non-correctly formatted code as broken.

To make sure only correctly formatted code is pushed to Github, it is recommended
to install a Git pre-commit hook that checks every local commit for correct code
formatting.

In this directory, we provide the [commit hook](pre-commit), can be installed by
copying the `pre-commit` file from this directory to `.git/hooks/pre-commit`.
Please note that there is also a separate commit hook for the enterprise repository,
which should be copied to `enterprise/.git/hooks/pre-commit`.

The commit hook will automatically verify that all to-be-committed files are
correctly formatted, and will abort the commit process in case of a formatting
violation.

To apply correct formatting, it is possible to run a
[script](../scripts/clang-format.sh) that we provide:
- `./scripts/clang-format.sh` in Bash on Linux and macOS
- `.\scripts\clang-format.ps1` in PowerShell on Windows (wrapper for `clang-format.sh`)

After running this script, the code should be correctly formatted. It is then
necessary to stage the formatting changes and commit again.
