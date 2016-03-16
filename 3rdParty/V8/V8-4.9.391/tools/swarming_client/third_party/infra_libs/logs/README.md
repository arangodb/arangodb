# Logging in infra.git

## Features

The `infra_libs.logs` package contains some code to simplify logging and
make it consistent and easily configurable. Using it makes your code
future-proof.

Offered features:

* log level can be changed from the command-line.
* too verbose modules can be blacklisted for easier debugging.
* ensures a consistent log format.

A typical log line looks like:

    [I2014-06-27T11:42:32.418716-07:00 7082 logs:71] this is the message

The first letter gives the severity of the message, followed by a
timestamp with timezone information (iso8601), the process id, the
current module name, and the thread id. After the closing square bracket
comes the actual message.

## Sample code

This is the standard way to set up logging so as to take advantage of
the goodness provided by `infra_libs.logs`.

In top-level files (other example in
[infra.services.sysmon.__main__](../../infra/services/sysmon/__main__.py)):

```python
import argparse
import infra_libs.logs

parser = argparse.ArgumentParser()
infra_libs.logs.add_argparse_options(parser)

options = parser.parse_args()
infra_libs.logs.process_argparse_options(options)
```

Logging messages should be done this way (other example in
`infra.libs.service_utils.outer_loop`):

```python
import logging
LOGGER = logging.getLogger(__name__)

LOGGER.info('great message')
LOGGER.error('terrible error')
```

Using `logging.getLogger` is a good practice in general (not restricted to
using infra_libs.logs) because it allows for module blacklisting and
other goodness. It should be done at import time. See also the official
[logging HOWTO](https://docs.python.org/2/howto/logging.html).
`infra_libs.logs` also formats the output of the root logger, but using
this logger is not recommended.
