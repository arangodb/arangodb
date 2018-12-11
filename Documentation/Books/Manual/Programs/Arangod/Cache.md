# ArangoDB Server Cache Options

Since ArangoDB 3.2, the several core components of the server use a cache
system which pools memory across many different cache tables. In order to
provide intelligent internal memory management, the system periodically
reclaims memory from caches which are used less often and reallocates it to
caches which get more activity.

## Rebalancing interval

Time between cache rebalancing attempts: `--cache.rebalancing-interval`

The value is specified in microseconds with a default of 2 seconds and a
minimum of 500 milliseconds.

## Cache size

Global size limit for all hash caches: `--cache.size`

The global caching system, all caches, and all the data contained therein will
fit inside this limit. The size is specified in bytes. If there is less than
4GiB of RAM on the system, the default value is 256MiB. If there is more,
the default is `(system RAM size - 2GiB) * 0.25`.
