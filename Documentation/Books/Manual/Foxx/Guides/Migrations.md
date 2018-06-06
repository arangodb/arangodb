# Lifecycle scripts and migrations

Bla bla `setup` is used automatically on install/upgrade/replace unless you disable via option. Use `teardown` to clean up when service is uninstalled.

Since the `setup` script can be run multiple times on an already installed service it's a good idea to make it reentrant (i.e. not result in errors if parts of it already have been run before).

The `teardown` script should remove all traces of the service for a clean uninstall. Keep in mind this results in catastrophic data loss. May want to rename to something else and invoke manually if that's too risky.

## Migrations

Running large `setup` on every upgrade can be very expensive during development. Alternative solution is to only handle basic collection creation and indexes and have one-off migration scripts and invoke them manually. Bla bla.
