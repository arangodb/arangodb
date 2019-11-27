# Parallel transfers

curl 7.66.0 introduces support for doing multiple transfers simultaneously; in
parallel.

## -Z, --parallel

When this command line option is used, curl will perform the transfers given
to it at the same time. It will do up to `--parallel-max` concurrent
transfers, with a default value of 50.

## Progress meter

The progress meter that is displayed when doing parallel transfers is
completely different than the regular one used for each single transfer.

  It shows:

 o percent download (if known, which means *all* transfers need to have a
   known size)
 o precent upload (if known, with the same caveat as for download)
 o total amount of downloaded data
 o total amount of uploaded data
 o number of transfers to perform
 o number of concurrent transfers being transferred right now
 o number of transfers queued up waiting to start
 o total time all transfers are expected to take (if sizes are known)
 o current time the transfers have spent so far
 o estimated time left (if sizes are known)
 o current transfer speed (the faster of UL/DL speeds measured over the last
   few seconds)

Example:

    DL% UL%  Dled  Uled  Xfers  Live   Qd Total     Current  Left    Speed
    72  --  37.9G     0   101    30    23  0:00:55  0:00:34  0:00:22 2752M

## Behavior differences

Connections are shared fine between different easy handles, but the
"authentication contexts" are not. So for example doing HTTP Digest auth with
one handle for a particular transfer and then continue on with another handle
that reuses the same connection, the second handle can't send the necessary
Authorization header at once since the context is only kept in the original
easy handle.

To fix this, the authorization state could be made possible to share with the
share API as well, as a context per origin + path (realm?) basically.

Visible in test 153, 1412 and more.

## Feedback!

This is early days for parallel transfer support. Keep your eyes open for
unintended side effects or downright bugs.

Tell us what you think and how you think we could improve this feature!

