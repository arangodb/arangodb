Long: retry-connrefused
Help: Retry on connection refused (use with --retry)
Added: 7.52.0
---
In addition to the other conditions, consider ECONNREFUSED as a transient
error too for --retry. This option is used together with --retry.
