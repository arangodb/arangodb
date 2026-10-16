# Differences between 3.12.10 and 3.12.11 with RBAC/Classic

This file lists actual differences which were introduced with the RBAC
code in comparison to the previous version. This list is supposed to
be extremely short, since it will only contain very badly bug fixes in
authorization behaviour and changes which are rather inconsequential
and have only been done to simplify things.

## Some error codes in /_arango/experimental

If a user has no database access to `_system` and calls

```GET /_arango/experimental/_admin/activities```

then we will now return with HTTP 404 "NOT FOUND" instead of HTTP 401
"FORBIDDEN". The error code will now be 1228 rather than 11.

This is because the experimental API version is considered to be
larger than 0 and we are doing this change from API V0 to API V1
for security reasons.

## Changes to access tokens in read-only mode

The two APIs:
 - `POST /_api/token/<username>`
 - `DELETE /_api/token/<username>/<id>`
up to 3.12.10 did not respect the read-only mode and allowed writes,
provided a user has RW access to the `_system` database. This is now
fixed and such an admin cannot create or delete tokens when the server
is in read-only mode. The response code is HTTP 403 with `errorNum`
1004 in this case. The superuser can still perform these operations,
even in read-only mode.

## ErrorNum in read-only mode

Essentially every API which writes anything is supposed to refuse work
if the server is in read-only mode. For those, which require write access
to a collection, the HTTP response code is HTTP 403 FORBIDDEN in the
read-only case. As part of the RBAC transitition, we have intentionally
changed the `errorNum` reported from 11 (FORBIDDEN) to 1004 (READONLY).
This happens only in the case that the user actually **has** RW access
to the collection, but the read-only mode prevents writes. The superuser
is not affected by this limitation.

## Fix a permission error

The two APIs:
- `PUT /_db/mydb/_api/query-cache/properties`
- `DELETE /_db/mydb/_api/query-cache`
up to 3.12.10 did not execute any permission check at all. Now they both 
requires write access to the `_system` database **and** read access to
`mydb` (which might be a consequence of the write access to `_system`,
if this is not explicitly overridden for `mydb`).
