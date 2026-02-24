# Versioned API

This describes our new approach to versioning the API in ArangoDB.

## Goals

We will give the API a version number, **which is independent of the
ArangoDB version number**.

A **breaking change** (also called an **incompatible change**) is any
modification to an API that causes existing client code to fail, behave
incorrectly, or require code changes to continue functioning (see the next
section for a definition). This will in particular consider that we
have drivers for typed languages, which use typed object serialization
and deserialization to/from JSON. Such drivers may not be able to handle
changes in the API that affect the structure of the JSON data they
receive or send.

Breaking changes require a **version increment** in the API versioning
scheme.

The fundamental idea is that client code sends the API version it requests
alongside the request in the URL path. This means that a new version of
ArangoDB can serve **the exact same API** as the previous version, when the
old API version is requested, and at the same time can provide a **new API**
for a later API version. This will finally allow seamless upgrades between
ArangoDB versions and explicit and well-documented API changes.

The current API will be API version 0 and the API of ArangoDB version 4.0.0
will be API version 1. Version 3.12.8 will offer API versions 0 and 1, and when
1 is requested, it will not serve the API endpoints which are removed for 4.0. 
This allows to request API version 1 in 3.12.8 (without a major version
upgrade) to see if anything would fail after an upgrade to 4.0.0.

In the future, we will offer newer API versions, but the a new API
version will not automatically include all API endpoints of the previous
version. Instead, it will only include the endpoints that have undergone
**breaking changes**. All unchanged API endpoints will be declined (with
an HTTP 404), if they have not explicitly been enabled in the new API
version.

This means that the latest API version will in a sense be "incomplete",
since it simply does not cover the complete functionality of the database.

However, we will only ever allow **the latest API version** to be incomplete.
For example, API version 2 can be incomplete, but a new API version 3 can only
appear when API version 2 is "complete". That is, if we want to release API
version 3, we must first copy all remaining, unchanged API endpoints from API
version 1 to API version 2, thereby making API version 2 complete.

## Definition of "breaking change"

The definitions in this section are made to achieve the following: If
the database is upgraded to a new version, which offers the same API
version as a previous version, then everything which worked before the
update will continue to work after the update. **The other direction
(downgrades) might not have this property!**

For example, adding a new API path is **not breaking** and thus does not
require an API version change. Of course, if somebody is using this path
in a newer version and then downgrades, this path might not work any more.

Note that the OpenAPI specification distinguishes between "required"
and "optional" attributes (or query parameters or HTTP headers), both
for the request data and the response data! An input attribute is
"required", if it leads to an error if it is missing. It is "optional",
if can be given but it does not lead to an error if it is missing. An
output attribute is "required", if a client can assume that it is always
present in the response. It is "optional", if the server reserves the
right to not include it in the response.

The following changes in an API are explicitly defined to be breaking,
and thus require an API version change:

  - Removing an API endpoint (in the sense that it now leads to HTTP 404 Not Found)
  
  - Attributes in a JSON body of a request:
      - Removing it (in the sense that it is no longer accepted)
      - Changing the data type or meaning
      - Adding a **required** attribute or making an optional 
        attribute required
    
  - URL parameters:
      - Removing it (in the sense that it is no longer accepted)
      - Changing the data type or meaning
      - Adding a **required** URL parameter or making an optional
        URL parameter required
  
  - HTTP headers starting with `x-arango-`:
      - Removing it (in the sense that is it no longer accepted)
      - Changing the data type or meaning
      - Adding a **required** such header or maing an optional
        header required (so far these do not exist)
 
  - Attributes in a JSON body of a response:
      - Removing a required one or making it optional
      - Changing the data type or meaning
      - Adding a new enum value for an existing attribute
    
  - Changing authentication/authorization
  
  - Making validation rules stricter

On the contrary, the following changes are **not** considered to be breaking:

  - Adding a new endpoint (URL postfix)
  - Adding an optional attribute to a JSON request body
  - Adding an optional URL query parameter
  - Adding an optional `x-arango-*` header
  - Adding an attribute (required or optional) to a response body
  - Adding a new enum value to a request (input!) JSON body attribute

Note that for attributes of object or array type non-breaking changes of
subattributes are allowed and do not count as "Changing the data type or
meaning"!

Note the asymmetry between input and output attributes:

The server is supposed to be very strict, decide between optional and
required input attributes, and must not accept unknown attributes. The
same is for URL query parameters and HTTP headers which start with
`x-arango-`. Other HTTP headers should not have an effect on the API
behaviour. Obviously, the `Content-Type`, `Accept` and `Content-Length`
headers are used and potentially other standard HTTP headers.

We will probably not be able to make the server completely strict for the 
4.0.0 version (and thus the API version 1). And since suddenly no longer
accepting some unknown attribute is akin to removing an attribute, such
"strictifications" will be considered breaking changes and thus require
a change in the API version!

Clients are supposed to be more lenient, it can only rely on the
existence of required output attributes. A client must accept and ignore
attributes it does not know.

It is possible that clients are not yet so flexible, so we should add
output attributes carefully!

## Implementation details

From 3.12.8 and 4.0.0 on, all API paths are versioned by prefixing them
with `/_arango/vX` where `X` is a non-negative decimal integer. That is,
`POST /_api/document` becomes `POST /_arango/v1/_api/document` (and analogously
for all existing API paths).

API version 0 is the current 3.12.8 API. API version 1 is the exact same API,
but with those things removed which we delete and cleanup for 4.0, that is,
API version 1 is the API of ArangoDB 4.0.0.

For now, all API paths can still be used without the prefix `/_arango/vX`,
but then this counts as if the API path were prefixed with `/_arango/v0` in 3.12.8
and future 3.12 versions, whereas it counts as `/_arango/v1` in 4.0.0 and future
4.0 versions (since 4.0.0 does not offer API version 0).

**IMPORTANT**: If we use one of the API calls which are removed for
4.0.0 in 3.12.8, but with prefix `/_arango/v1`, then the answer is HTTP
404 Not found, exactly as in ArangoDB 4.0.0. This allows to "try out"
the 4.0.0 API in 3.12.8 (and later 3.12 versions). The removed APIs have
to be called without version prefix or with `/_arango/v0` in 3.12.8 and
future 3.12 versions.

Other than that, API versions 0 and 1 are not different.

API version 1 is allowed to add stricter checking in comparison to
API version 0 (see above), but then this stricter checking must be
implemented in 3.12.8, too, if API version 1 is used.

There shall be a prefix of `/_arango/experimental` which we can use to
implement experimental APIs. These APIs are not guaranteed to be compatible
between ArangoDB versions and are intended for testing and development purposes.
The prefix `/_arango/experimental` will only give access to experimental APIs.

If a database is specified in the API path, then the `/_db/<database-name>` follows
the version prefix `/_arango/vX` and comes before the actual API path:

```GET /_arango/v1/_db/mydb/_api/document/coll/xyz```

is the right way to fetch the document with `_key` `xyz` from the collection
`coll` in the database `mydb`. If the `/_db/<database-name>` is omitted, then
the document is fetched from the `_system` database.

In the future, we also want to support the prefix `/_internal/vX` for internal
APIs. These APIs are not described in the OpenAPI specification (see below), are
not documented and are supposed to be used only internally within an ArangoDB
cluster. This will not be implemented for 3.12.8 and 4.0.0. The idea is that
on the platform envoy does **not** forward requests with this prefix to ArangoDB,
so no outside user can in fact use these APIs.

For internal APIs we can relax the deprecation policies a bit (see below).

## Rules for API versioning

1. If the API is changed with a **breaking** change, the API version
   must be incremented.
2. The API version applies **globally** to all endpoints.
3. The latest API version offered by an ArangoDB version may be incomplete,
   in that it only contains those API endpoints that have undergone
   breaking changes from the previous version.
4. An ArangoDB version can implement multiple API versions and must
   report these in the response to the version API (details see below).
5. Non-breaking changes are allowed from one version to another without
   changing the API version.
6. If a feature or bugfix involves a breaking change and thus an API version
   change, it must not be backported to older versions.

## Deprecation policy

1. API versions can be deprecated and this can happen with a **minor** or
   **major** version change, but only, if the smaller of the two ArangoDB
   versions already has a newer API version available.
2. A deprecated API version can only be removed with a new **major** ArangoDB
   version, and only if it has been deprecated for **all versions of the previous
   major ArangoDB version**.
3. When we have LTS (long term support) versions, then every new LTS version must
   still support the latest API version that the previous LTS version supported
   at the time when the new LTS version was released.
4. It is not considered to be a breaking change if an **internal** API endpoint is
   removed in an API version from one ArangoDB version to another. Such an endpoint
   should first be deprecated (marked as deprecated in the code) to indicate that
   it should no longer be used. This should only be done if
     - there is a new endpoint in the same API version which replaces the old one, or
     - there is a new endpoint in a new API version, which replaces the old one, or
     - the functionality will no longer be supported in the future.
   For all such changes we need to consider smooth rolling upgrades, that is situations,
   in which some instances of a cluster are already upgraded and some are still running
   the previous version. We also need to consider LTS versions.

## Finding out about API versions

The API `GET /_api/version` will remain unversioned forever and shall
return a JSON document with at least the following attributes:

```json
{
  "apiVersions": ["v4", "v3", "v2"],
  "deprecatedApiVersions": ["v2"]
}
```

and the supported API versions shall be sorted in descending numerical order,
so that `apiVersions[0]` is the latest officially supported API version.

This allows to use `GET /_api/version` as a good entry point for drivers
to find out about the latest API version supported by the server. It is also possible
to use `GET /_arango/vX/_api/version` with any `X` that is a supported API version
and the output in other attributes can be different for different API versions,
but the above mentioned attributes are guaranteed to be the same for all API versions
and `GET /_api/version` will continue to work forever.

## Strictness rules in the server

We strive to **enforce** - over time - the following **strictness rules** in the server:

 - The server must reject requests with an API version that is not supported.
 - The server should reject requests with a JSON body that contains an unknown
   attribute, uses an unknown query parameter or uses an unknown HTTP header which
   begins with `x-arango-`.
 - The server should reject an unknown API path with HTTP 404 Not found.

If ArangoDB 4.0.0 (and thus API version 1) does not fully enforce these, we can add
checks later, but this will require a new API version. We can for example do this
in API version 2.

## Client and driver requirements

Clients should be **lenient** when parsing HTTP responses:

 - Clients should ignore unknown attributes in JSON responses.
 - Clients should ignore unknown HTTP headers in HTTP responses which
   begin with `x-arango-`.

## Discoverability via OpenAPI

Our old endpoint for swagger `/_db/_system/_admin/aardvark/api/swagger.json` no longer
exists. We should make the API discoverable via OpenAPI and use

```
/_arango/Vx/openapi.json
```

as the entry point. It delivers a JSON file with `Content-Type: application/json`.
For Versions 3.12.8 (and later) and 4.0.0 we can deliver a static JSON file which
we prepare exactly as we did so for earlier ArangoDB versions. Moving forward,
we can produce a router library, which can be used to generate the OpenAPI
description file dynamically.

This endpoint should deliver the OpenAPI description for exactly the requested
API version (if supported by the ArangoDB version).

This is the way to discover differences in API in a given API version (which could
exist because of non-breaking changes), and to discover which API endpoints are
available in which API version. For example, it is possible to discover an
"incomplete" API version in this way.
