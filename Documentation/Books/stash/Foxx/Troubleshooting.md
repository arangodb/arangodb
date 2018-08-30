Troubleshooting
===============

Check arangod.log for errors

Make sure to check surrounding stacktraces because they may be related

When catching ArangoError exceptions use `console.errorStack(err)` to log as internal errors may have been rethrown and that function preserves the full stack trace.

Common errors/problems observed by community and how to solve?