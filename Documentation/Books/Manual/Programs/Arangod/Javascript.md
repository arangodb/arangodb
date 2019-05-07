# ArangoDB Server JavaScript Options

## JavaScript code execution

`--javascript.allow-admin-execute`

This option can be used to control whether user-defined JavaScript code
is allowed to be executed on server by sending via HTTP to the API endpoint
`/_admin/execute`  with an authenticated user account.
The default value is *false*, which disables the execution of user-defined
code. This is also the recommended setting for production. In test environments,
it may be convenient to turn the option on in order to send arbitrary setup
or teardown commands for execution on the server.

## V8 contexts

`--javascript.v8-contexts number`

Specifies the maximum *number* of V8 contexts that are created for executing
JavaScript code. More contexts allow executing more JavaScript actions in
parallel, provided that there are also enough threads available. Please note
that each V8 context will use a substantial amount of memory and requires
periodic CPU processing time for garbage collection.

Note that this value configures the maximum number of V8 contexts that can be
used in parallel. Upon server start only as many V8 contexts will be created as
are configured in option `--javascript.v8-contexts-minimum`. The actual number of
available V8 contexts may float at runtime between `--javascript.v8-contexts-minimum`
and `--javascript.v8-contexts`. When there are unused V8 contexts that linger around,
the server's garbage collector thread will automatically delete them.

`--javascript.v8-contexts-minimum number`

Specifies the minimum *number* of V8 contexts that will be present at any time
the server is running. The actual number of V8 contexts will never drop below this
value, but it may go up as high as specified via the option `--javascript.v8-contexts`.

When there are unused V8 contexts that linger around and the number of V8 contexts
is greater than `--javascript.v8-contexts-minimum` the server's garbage collector
thread will automatically delete them.

`--javascript.v8-contexts-max-invocations`

Specifies the maximum number of invocations after which a used V8 context is
disposed. The default value of `--javascript.v8-contexts-max-invocations` is 0,
meaning that the maximum number of invocations per context is unlimited.

`--javascript.v8-contexts-max-age`

Specifies the time duration (in seconds) after which time a V8 context is disposed
automatically after its creation. If the time is elapsed, the context will be disposed.
The default value for `--javascript.v8-contexts-max-age` is 60 seconds.

If both `--javascript.v8-contexts-max-invocations` and `--javascript.v8-contexts-max-age`
are set, then the context will be destroyed when either of the specified threshold
values is reached.

## Garbage collection frequency (time-based)

`--javascript.gc-frequency frequency`

Specifies the frequency (in seconds) for the automatic garbage collection of
JavaScript objects. This setting is useful to have the garbage collection still
work in periods with no or little numbers of requests.

## Garbage collection interval (request-based)

`--javascript.gc-interval interval`

Specifies the interval (approximately in number of requests) that the garbage
collection for JavaScript objects will be run in each thread.

## V8 options

`--javascript.v8-options options`

Optional arguments to pass to the V8 Javascript engine. The V8 engine will run
with default settings unless explicit options are specified using this
option. The options passed will be forwarded to the V8 engine which will parse
them on its own. Passing invalid options may result in an error being printed on
stderr and the option being ignored.

Options need to be passed in one string, with V8 option names being prefixed
with double dashes. Multiple options need to be separated by whitespace. To get
a list of all available V8 options, you can use the value *"--help"* as follows:

```
--javascript.v8-options="--help"
```

Another example of specific V8 options being set at startup:

```
--javascript.v8-options="--log"
```

Names and features or usable options depend on the version of V8 being used, and
might change in the future if a different version of V8 is being used in
ArangoDB. Not all options offered by V8 might be sensible to use in the context
of ArangoDB. Use the specific options only if you are sure that they are not
harmful for the regular database operation.

### Enable or Disable V8 JavaScript Engine entirely

```
--javascript.enabled bool
```

In certain types of ArangoDB instances you can now completely disable the V8
JavaScript engine. Be aware that this is an **highly experimental** feature and
it is to be expected that certain functionality (e.g. some API endpoints, the
WebUI, some AQL functions etc) will be missing or severely broken. Nevertheless
you may wish to reduce the footprint of ArangoDB by disabling V8.

This option is expected to **only** work reliably on a _single server_, _DBServer_,
or _agency_. Do not try to use this feature on a _coordinator_ or in the _ActiveFailover_ setup.

### Copy JavaScript Installation files

```
--javascript.copy-installation bool
```

Copy contents of 'javascript.startup-directory' on first start of the server. This option
is intended to be useful for _rolling upgrades_. Setting this to _true_ means that you can
upgrade the underlying ArangoDB packages, without influencing the running _arangod_ instance.
Setting this value does only make sense if you use ArangoDB outside of a container solution,
like Docker, Kubernetes, etc.
