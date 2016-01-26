

@brief log dates and times in local time zone
`--log.use-local-time`

If specified, all dates and times in log messages will use the server's
local time-zone. If not specified, all dates and times in log messages
will be printed in UTC / Zulu time. The date and time format used in logs
is always `YYYY-MM-DD HH:MM:SS`, regardless of this setting. If UTC time
is used, a `Z` will be appended to indicate Zulu time.

