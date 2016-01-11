

@brief log thread identifier
`--log.thread`

Whenever log output is generated, the process ID is written as part of the
log information. Setting this option appends the thread id of the calling
thread to the process id. For example,

```
2010-09-20T13:04:01Z [19355] INFO ready for business
```

when no thread is logged and

```
2010-09-20T13:04:17Z [19371-18446744072487317056] ready for business
```

when this command line option is set.

