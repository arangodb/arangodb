# ArangoDB Server LDAP Options

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

## Basics Concepts

The basic idea is that one can keep the user authentication setup for
an ArangoDB instance (single or cluster) outside of ArangoDB in an LDAP
server. A crucial feature of this is that one can add and withdraw users
and permissions by only changing the LDAP server and in particular
without touching the ArangoDB instance. Changes will be effective in
ArangoDB within a few minutes.

Since there are many different possible LDAP setups, we must support a
variety of possibilities for authentication and authorization. Here is
a short overview:

To map ArangoDB user names to LDAP users there are two authentication
methods called "simple" and "search". In the "simple" method the LDAP bind
user is derived from the ArangoDB user name by prepending a prefix and
appending a suffix. For example, a user "alice" could be mapped to the
distinguished name `uid=alice,dc=arangodb,dc=com` to perform the LDAP
bind and authentication.
See [Simple authentication method](#simple-authentication-method)
below for details and configuration options.

In the "search" method there are two phases. In Phase 1 a generic
read-only admin LDAP user account is used to bind to the LDAP server
first and search for an LDAP user matching the ArangoDB user name. In
Phase 2, the actual authentication is then performed against the LDAP
user that was found in phase 1. Both methods are sensible and are
recommended to use in production.
See [Search authentication method](#search-authentication-method)
below for details and configuration options.

Once the user is authenticated, there are now two methods for
authorization: (a) "roles attribute" and (b) "roles search".

In method (a) ArangoDB acquires a list of roles the authenticated LDAP
user has from the LDAP server. The actual access rights to databases
and collections for these roles are configured in ArangoDB itself.
The user effectively has the union of all access rights of all roles
he has. This method is probably the most common one for production use
cases. It combines the advantages of managing users and roles outside of
ArangoDB in the LDAP server with the fine grained access control within
ArangoDB for the individual roles. See [Roles attribute](#roles-attribute)
below for details about method (a) and for the associated configuration
options.

Method (b) is very similar and only differs from (a) in the way the
actual list of roles of a user is derived from the LDAP server. 
See [Roles search](#roles-search) below for details about method (b)
and for the associated configuration options.


Fundamental options
-------------------

The fundamental options for specifying how to access the LDAP server are
the following:

  - `--ldap.enabled` this is a boolean option which must be set to
    `true` to activate the LDAP feature
  - `--ldap.server` is a string specifying the host name or IP address
    of the LDAP server
  - `--ldap.port` is an integer specifying the port the LDAP server is
    running on, the default is *389*
  - `--ldap.basedn` specifies the base distinguished name under which
    the search takes place (can alternatively be set via `--ldap.url`)
  - `--ldap.binddn` and `--ldap.bindpasswd` are distinguished name and
    password for a read-only LDAP user to which ArangoDB can bind to
    search the LDAP server. Note that it is necessary to configure these
    for both the "simple" and "search" authentication methods, since
    even in the "simple" method, ArangoDB occasionally has to refresh
    the authorization information from the LDAP server
    even if the user session persists and no new authentication is
    needed! It is, however, allowed to leave both empty, but then the
    LDAP server must be readable with anonymous access.
  - `--ldap.refresh-rate` is a floating point value in seconds. The
    default is 300, which means that ArangoDB will refresh the
    authorization information for authenticated users after at most 5
    minutes. This means that changes in the LDAP server like removed
    users or added or removed roles for a user will be effective after
    at most 5 minutes.

Note that the `--ldap.server` and `--ldap.port` options can
alternatively be specified in the `--ldap.url` string together with
other configuration options. For details see Section "LDAP URLs" below.

Here is an example on how to configure the connection to the LDAP server,
with anonymous bind:

    --ldap.enabled=true \
    --ldap.server=ldap.arangodb.com \
    --ldap.basedn=dc=arangodb,dc=com

With this configuration ArangoDB binds anonymously to the LDAP server
on host `ldap.arangodb.com` on the default port 389 and executes all searches
under the base distinguished name `dc=arangodb,dc=com`.

If we need a user to read in LDAP here is the example for it:

    --ldap.enabled=true \
    --ldap.server=ldap.arangodb.com \
    --ldap.basedn=dc=arangodb,dc=com \
    --ldap.binddn=uid=arangoadmin,dc=arangodb,dc=com \
    --ldap.bindpasswd=supersecretpassword

The connection is identical but the searches will be executed with the
given distinguished name in `binddn`.

Note here:
The given user (or the anonymous one) needs at least read access on
all user objects to find them and in the case of Roles search
also read access on the objects storing the roles.

Up to this point ArangoDB can now connect to a given LDAP server
but it is not yet able to authenticate users properly with it.
For this pick one of the following two authentication methods.

### LDAP URLs

As an alternative one can specify the values of multiple LDAP related configuration
options by specifying a single LDAP URL. Here is an example:

    --ldap.url ldap://ldap.arangodb.com:1234/dc=arangodb,dc=com?uid?sub

This one option has the combined effect of setting the following:

    --ldap.server=ldap.arangodb.com \
    --ldap.port=1234 \
    --ldap.basedn=dc=arangodb,dc=com \
    --ldap.searchAttribute=uid \
    --ldap.searchScope=sub

That is, the LDAP URL consists of the LDAP *server* and *port*, a *basedn*, a
*search attribute* and a *scope* which can be one of *base*, *one* or
*sub*. There is also the possibility to use the `ldaps` protocol as in:

    --ldap.url ldaps://ldap.arangodb.com:636/dc=arangodb,dc=com?uid?sub

This does exactly the same as the one above, except that it uses the
LDAP over TLS protocol. This is a non-standard method which does not
involve using the STARTTLS protocol. Note that this does not work in the
Windows version! We suggest to use the `ldap` protocol and STARTTLS
as described in the next section.

### TLS options

{% hint 'warning' %}
TLS is not supported in the Windows version of ArangoDB!
{% endhint %}

To configure the usage of encrypted TLS to communicate with the LDAP server
the following options are available:

  - `--ldap.tls`: the main switch to active TLS. can either be 
      `true` (use TLS) or `false` (do not use TLS). It is switched
      off by default. If you switch this on and do not use the `ldaps`
      protocol via the [LDAP URL](#ldap-urls), then ArangoDB
      will use the `STARTTLS` protocol to initiate TLS. This is the
      recommended approach.
  - `--ldap.tls-version`: the minimal TLS version that ArangoDB should accept.
      Available versions are `1.0`, `1.1` and `1.2`. The default is `1.2`. If
      your LDAP server does not support Version 1.2, you have to change
      this setting.
  - `--ldap.tls-cert-check-strategy`: strategy to validate the LDAP server
      certificate.  Available strategies are `never`, `hard`,
      `demand`, `allow` and `try`. The default is `hard`.
  - `--ldap.tls-cacert-file`: a file path to one or more (concatenated)
      certificate authority certificates in PEM format.
      As default no file path is configured. This certificate
      is used to validate the server response.
  - `--ldap.tls-cacert-dir`: a directory path to certificate authority certificates in
      [c_rehash](https://www.openssl.org/docs/man1.0.2/apps/c_rehash.html)
      format. As default no directory path is configured.

Assuming you have the TLS CAcert file that is given to the server at
`/path/to/certificate.pem`, here is an example on how to configure TLS:


    --ldap.tls true \
    --ldap.tls-cacert-file /path/to/certificate.pem

You can use TLS with any of the following authentication mechanisms.

### Esoteric options

The following options can be used to configure advanced options for LDAP
connectivity: 
  
  - `--ldap.serialized`: whether or not calls into the underlying LDAP library should be serialized.
    This option can be used to work around thread-unsafe LDAP library functionality.
  - `--ldap.serialize-timeout`: sets the timeout value that is used when waiting to enter the 
    LDAP library call serialization lock. This is only meaningful when `--ldap.serialized` has been
    set to `true`. 
  - `--ldap.retries`: number of tries to attempt a connection. Setting this to values greater than
    one will make ArangoDB retry to contact the LDAP server in case no connection can be made
    initially.

Please note that some of the following options are platform-specific and may not work
with all LDAP servers reliably:

  - `--ldap.restart`: whether or not the LDAP library should implicitly restart connections
  - `--ldap.referrals`: whether or not the LDAP library should implicitly chase referrals

The following options can be used to adjust the LDAP configuration on Linux and MacOS 
platforms only, but will not work on Windows:

  - `--ldap.debug`: turn on internal OpenLDAP library output (warning: will print to stdout).
  - `--ldap.timeout`: timeout value (in seconds) for synchronous LDAP API calls (a value of 0 
    means default timeout).
  - `--ldap.network-timeout`: timeout value (in seconds) after which network operations 
    following the initial connection return in case of no activity (a value of 0 means default timeout).
  - `--ldap.async-connect`: whether or not the connection to the LDAP library will be done 
    asynchronously.

## Authentication methods

In order to authenticate users in LDAP we have two options available.
We need to pick exactly one them.

### Simple authentication method

The simple authentication method is used if and only if both the
`--ldap.prefix` and `--ldap.suffix` configuration options are specified
and are non-empty. In all other cases the
["search" authentication method](#search-authentication-method) is used.

In the "simple" method the LDAP bind user is derived from the ArangoDB
user name by prepending the value of the `--ldap.prefix` configuration
option and by appending the value of the `--ldap.suffix` configuration
option. For example, an ArangoDB user "alice" would be mapped to the
distinguished name `uid=alice,dc=arangodb,dc=com` to perform the LDAP
bind and authentication, if `--ldap.prefix` is set to `uid=` and
`--ldap.suffix` is set to `,dc=arangodb,dc=com`.

ArangoDB binds to the LDAP server and authenticates with the
distinguished name and the password provided by the client. If
the LDAP server successfully verifies the password then the user is 
authenticated.

If you want to use this method add the following example to your
ArangoDB configuration together with the fundamental configuration:

    --ldap.prefix uid= \
    --ldap.suffix ,dc=arangodb,dc=com

This method will authenticate an LDAP user with the distinguished name
`{PREFIX}{USERNAME}{SUFFIX}`, in this case for the arango user `alice`
it will search for: `uid=alice,dc=arangodb,dc=com`.
This distinguished name will be used as `{{USER}}` for the roles later on.

### Search authentication method

The search authentication method is used if at least one of the two
options `--ldap.prefix` and `--ldap.suffix` is empty or not specified.
ArangoDB uses the LDAP user credentials given by the `--ldap.binddn` and
`--ldap.bindpasswd` to perform a search for LDAP users.
In this case, the values of the options `--ldap.basedn`,
`--ldap.search-attribute`, `--ldap.search-filter` and `--ldap.search-scope`
are used in the following way:

  - `--ldap.search-scope` is an LDAP search scope with possible values
    `base` (just search the base distinguished name),
    `sub` (recursive search under the base distinguished name) or
    `one` (search the base's immediate children) (default: `sub`)
  - `--ldap.search-filter` is an LDAP filter expression which limits the
    set of LDAP users being considered (default: `objectClass=*` which
    means all objects)
  - `--ldap.search-attribute` specifies the attribute in the user objects
    which is used to match the ArangoDB user name (default: `uid`)

Here is an example on how to configure the search method.
Assume we have users like the following stored in LDAP:

    dn: uid=alice,dc=arangodb,dc=com
    uid: alice
    objectClass: inetOrgPerson
    objectClass: organizationalPerson
    objectClass: top
    objectClass: person 

Where `uid` is the username used in ArangoDB, and we only search
for objects of type `person` then we can add the following to our
fundamental LDAP configuration:

    --ldap.search-attribute=uid \
    --ldap.search-filter=objectClass=person

This will use the `sub` search scope by default and will find
all `person` objects where the `uid` is equal to the given username.
From these the `dn` will be extracted and used as `{{USER}}` in
the roles later on.

## Fetching roles for a user

After authentication, the next step is to derive authorization
information from the authenticated LDAP user.
In order to fetch the roles and thereby the access rights
for a user we again have two possible options and need to pick
one of them. We can combine each authentication method
with each role method.
In any case a user can have no role or more than one.
If a user has no role the user will not get any access
to ArangoDB at all.
If a user has multiple roles with different rights
then the rights will be combined and the `strongest`
right will win. Example:

- `alice` has the roles `project-a` and `project-b`.
- `project-a` has no access to collection `BData`.
- `project-b` has `rw` access to collection `BData`,
- hence `alice` will have `rw` on `BData`.

Note that the actual database and collection access rights
will be configured in ArangoDB itself by roles in the users module.
The role name is always prefixed with `:role:`, e.g.: `:role:project-a`
and `:role:project-b` respectively. You can use the normal user
permissions tools in the Web interface or `arangosh` to configure these.

### Roles attribute

The most important method for this is to read off the roles an LDAP
user is associated with from an attribute in the LDAP user object.
If the configuration option

    --ldap.roles-attribute-name

configuration option is set, then the value of that
option is the name of the attribute being used.

Here is the example to add to the overall configuration:

    --ldap.roles-attribute-name=role

If we have the user stored like the following in LDAP:

    dn: uid=alice,dc=arangodb,dc=com
    uid: alice
    objectClass: inetOrgPerson
    objectClass: organizationalPerson
    objectClass: top
    objectClass: person 
    role: project-a
    role: project-b

Then the request will grant the roles `project-a` and `project-b`
for the user `alice` after successful authentication,
as they are stored within the `role` on the user object.

### Roles search

An alternative method for authorization is to conduct a search in the
LDAP server for LDAP objects representing roles a user has. If the
configuration option

    --ldap.roles-search=<search-expression>

is given, then the string `{USER}` in `<search-expression>` is replaced
with the distinguished name of the authenticated LDAP user and the
resulting search expression is used to match distinguished names of
LDAP objects representing roles of that user.

Example:

    --ldap.roles-search '(&(objectClass=groupOfUniqueNames)(uniqueMember={USER}))'

After a LDAP user was found and authenticated as described in the
authentication section above the `{USER}` in the search expression
will be replaced by its distinguished name, e.g. `uid=alice,dc=arangodb,dc=com`,
and thus with the above search expression the actual search expression
would end up being:

    (&(objectClass=groupOfUniqueNames)(uniqueMember=uid=alice,dc=arangodb,dc=com}))

This search will find all objects of `groupOfUniqueNames` where
at least one `uniqueMember` has the `dn` of `alice`.
The list of results of that search would be the list of roles given by
the values of the `dn` attributes of the found role objects.

### Role transformations and filters

For both of the above authorization methods there are further
configuration options to tune the role lookup. In this section we
describe these further options:

  - `--ldap.roles-include` can be used to specify a regular expression
    that is used to filter roles. Only roles that match the regular
    expression are used.

  - `--ldap.roles-exclude` can be used to specify a regular expression
    that is used to filter roles. Only roles that do not match the regular
    expression are used.

  - `--ldap.roles-transformation` can be used to specify a regular
    expression and replacement text as `/re/text/`. This regular
    expression is applied to the role name found. This is especially
    useful in the roles-search variant to extract the real role name
    out of the `dn` value.

  - `--ldap.superuser-role` can be used to specify the role associated
    with the superuser. Any user belonging to this role gains superuser
    status. This role is checked after applying the roles-transformation
    expression.

Example:

    --ldap.roles-include "^arangodb" 

will only consider roles that start with `arangodb`.

    --ldap.roles-exclude=disabled

will only consider roles that do contain the word `disabled`.

    --ldap.superuser-role "arangodb-admin" 

anyone belonging to the group "arangodb-admin" will become a superuser.

The roles-transformation deserves a larger example. Assume we are using
roles search and have stored roles in the following way:

    dn: cn=project-a,dc=arangodb,dc=com
    objectClass: top
    objectClass: groupOfUniqueNames
    uniqueMember: uid=alice,dc=arangodb,dc=com
    uniqueMember: uid=bob,dc=arangodb,dc=com
    cn: project-a
    description: Internal project A

    dn: cn=project-b,dc=arangodb,dc=com
    objectClass: top
    objectClass: groupOfUniqueNames
    uniqueMember: uid=alice,dc=arangodb,dc=com
    uniqueMember: uid=charlie,dc=arangodb,dc=com
    cn: project-b
    description: External project B

In this case we will find `cn=project-a,dc=arangodb,dc=com` as one
role of `alice`. However we actually want to configure a role name:
`:role:project-a` which is easier to read and maintain for our
administrators.

If we now apply the following transformation:

    --ldap.roles-transformation=/^cn=([^,]*),.*$/$1/

The regex will extract out `project-a` resp. `project-b` of the
`dn` attribute.

In combination with the `superuser-role` we could make all
`project-a` members arangodb admins by using:

    --ldap.roles-transformation=/^cn=([^,]*),.*$/$1/ \
    --ldap.superuser-role=project-a


## Complete configuration examples

In this section we would like to present complete examples
for a successful LDAP configuration of ArangoDB.
All of the following are just combinations of the details described above.

**Simple authentication with role-search, using anonymous LDAP user**

This example connects to the LDAP server with an anonymous read-only
user. We use the simple authentication mode (`prefix` + `suffix`)
to authenticate users and apply a role search for `groupOfUniqueNames` objects
where the user is a `uniqueMember`. Furthermore we extract only the `cn`
out of the distinguished role name.

    --ldap.enabled=true \
    --ldap.server=ldap.arangodb.com \
    --ldap.basedn=dc=arangodb,dc=com \
    --ldap.prefix uid= \
    --ldap.suffix ,dc=arangodb,dc=com \
    --ldap.roles-search '(&(objectClass=groupOfUniqueNames)(uniqueMember={USER}))' \
    --ldap.roles-transformation=/^cn=([^,]*),.*$/$1/ \
    --ldap.superuser-role=project-a

**Search authentication with roles attribute using LDAP admin user having TLS enabled**

This example connects to the LDAP server with a given distinguished name of an
admin user + password.
Furthermore we activate TLS and give the certificate file to validate server responses.
We use the search authentication searching for the `uid` attribute of `person` objects.
These `person` objects have `role` attribute(s) containing the role(s) of a user. 

    --ldap.enabled=true \
    --ldap.server=ldap.arangodb.com \
    --ldap.basedn=dc=arangodb,dc=com \
    --ldap.binddn=uid=arangoadmin,dc=arangodb,dc=com \
    --ldap.bindpasswd=supersecretpassword \
    --ldap.tls true \
    --ldap.tls-cacert-file /path/to/certificate.pem \
    --ldap.search-attribute=uid \
    --ldap.search-filter=objectClass=person \
    --ldap.roles-attribute-name=role
