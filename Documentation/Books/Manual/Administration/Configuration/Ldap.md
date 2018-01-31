LDAP
====

__This feature is only available in the Enterprise Edition.__

Basics Concepts
---------------

The basic idea is that one can keep the user authentication setup for
an ArangoDB instance (single or cluster) outside of ArangoDB in an LDAP
server. A crucial feature of this is that one can add and withdraw users
and permissions by only changing the LDAP server and in particular
without touching the ArangoDB instance. Changes will be effective in
ArangoDB within a few minutes.

Since there are many different possible LDAP setups, we must support a
variety of possibilities for authentication and authorization. Here is
a short overview, for details see below:

To map ArangoDB user names to LDAP users there are two authentication
methods called "simple" and "search". In the "simple" method the LDAP bind
user is derived from the ArangoDB user name by prepending a prefix and
appending a suffix. For example, a user "alice" could be mapped to the
distinguished name `uid=alice,dc=arangodb,dc=com` to perform the LDAP
bind and authentication. See Section "Simple authentication method" below for
details and configuration options.

In the "search" method there are two phases. In Phase 1 a generic
read-only admin LDAP user account is used to bind to the LDAP server
first and search for an LDAP user matching the ArangoDB user name. In
Phase 2, the actual authentication is then performed against the LDAP
user that was found in phase 1. Both methods are sensible and are
recommended to use in production. See Section "Search authentication
method" below for details and configuration options.

Once the user is authenticated, there are now two methods for
authorization: (a) "roles attribute" and (b) "roles search".

In method (a) ArangoDB acquires a list of roles the authenticated LDAP
user has from the LDAP server. The actual access rights to databases
and collections for these roles are configured in ArangoDB itself.
The user effectively has the union of all access rights of all roles
he has. This method is probably the most common one for production use
cases. It combines the advantages of managing users and roles outside of
ArangoDB in the LDAP server with the fine grained access control within
ArangoDB for the individual roles. See Section "Roles attribute" below
for details about method (a) and for the associated configuration
options.

Method (b) is very similar and only differs from (a) in the way the
actual list of roles of a user is derived from the LDAP server. Note
that (b) is only possible with the "search" authentication method above.
See Section "Roles search" below for details about method (b) and for
the associated configuration options.


Fundamental options to find and connect to the LDAP server
----------------------------------------------------------

The fundamental options for specifying how to access the LDAP server are
the following:

  - `--ldap.enabled` this is a boolean option which must be set to
    `true` to activate the LDAP feature
  - `--ldap.server` is a string specifying the host name or IP address
    of the LDAP server
  - `--ldap.port` is an integer specifying the port the LDAP server is
    running on, the default is *389*
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


### Simple authentication method

The simple authentication method is used if and only if both the
`--ldap.prefix` and `--ldap.suffix` configuration options are specified
and are non-empty. In all other cases the "search" authentication method
described in the following section is used.

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


### Search authentication method

The search authentication method is used if at least one of the two
options `--ldap.prefix` and `--ldap.suffix` is empty or not specified.
ArangoDB uses the LDAP user credentials given by the `--ldap.binddn` and
`--ldap.bindpasswd` to perform a search for LDAP users.
In this case, the values of the options `--ldap.basedn`,
`--ldap.searchAttribute`, `--ldap.searchFilter` and `--ldap.searchScope`
are used in the following way:

  - `--ldap.basedn` specifies the base distinguished name under which
    the search takes place (can alternatively be set via `--ldap.url`)
  - `--ldap.searchScope` is an LDAP search scope with possible values
    `base` (just search the base distinguished name),
    `sub` (recursive search under the base distinguished name) or
    `one` (search the base's immediate children) (default: `sub`)
  - `--ldap.searchFilter` is an LDAP filter expression which limits the
    set of LDAP users being considered (default: `objectClass=*` which
    means all objects)
  - `--ldap.searchAttribute` specifies the attribute in the user objects
    which is used to match the ArangoDB user name (default: `uid`)

Here is an example with an anonymous bind:

    --ldap.enabled=true --ldap.server=ldap.arangodb.com \
    --ldap.basedn=dc=arangodb,dc=com \
    --ldap.roles-attribute-name=sn

With this configuration ArangoDB binds anonymously to the LDAP server
on host `ldap.arangodb.com` on the default port 389 and searches for an
LDAP user under the base distinghuished name `dc=arangodb,dc=com` whose 
`uid` attribute is equal to the ArangoDB user name given by the client. 
If such an LDAP user is found an authentication is
done with this LDAP user's DN and the password given by the client.
If authenticated, this LDAP user is used to fetch the authorization
information by using the roles given in the `sn` attribute (see below).

Here is an example with base distinguished name and a read-only admin
LDAP user and password:

    --ldap.enabled true --ldap.server ldap.arangodb.com \
    --ldap.basedn dc=arangodb,dc=com \
    --ldap.searchAttribute=loginname \
    --ldap.binddn cn=admin,dc=company,dc=com \
    --ldap.bindpasswd secretpassword \
    --ldap.roles-attribute-name=roles

With this configuration ArangoDB binds with `--ldap.binddn` and
`--ldap.bindpasswd` to the LDAP server and searches for a user whose
`loginname` attribute is equal to the ArangoDB user name. If
the LDAP user is found an authentication is done with the LDAP users DN and
the password given by the client, and then the list of the user's roles
is fetched from the `roles` attribute of the user.


### Roles attribute

After authentication, the next step is to derive authorization
information from the authenticated LDAP user. The most important method
for this is to read off the roles an LDAP user is associated with from
an attribute in the LDAP user object. If the
`--ldap.roles-attribute-name` configuration option is set, then the
value of that option is the name of the attribute being used.

Here is an example:

    --ldap.enabled=true --ldap.server=ldap.arangodb.com \
    --ldap.basedn=dc=arangodb,dc=com \
    --ldap.roles-attribute-name=role

If one uses `fermi` as the ArangoDB user name the following LDAP user
object will match and thus the roles `project-a` and `project-b` will
be associated with the authenticated user:

    dn: uid=fermi,dc=arangodb,dc=com
    uid: fermi
    role: project-a
    role: project-b

ArangoDB tries to authenticate with this LDAP user against the LDAP
server and searches for the roles in the attribute `role`. This will
give the user the combined permissions of the roles `project-a` and
`project-b`. Note that the actual database and collection access rights
will be configured in ArangoDB itself by documents in the `_users`
collection whose `name` attribute is `:role:project-a` and
`:role:project-b` respectively. You can use the normal user
permissions tools in the UI or the `arangosh` to configure these.


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

    --ldap.enabled true --ldap.server ldap.arangodb.com \
    --ldap.basedn dc=arangodb,dc=com \
    --ldap.binddn cn=admin,dc=arangodb,dc=com \
    --ldap.bindpasswd admin \
    --ldap.roles-search '(&(objectClass=groupOfUniqueNames)(uniqueMember={USER}))'

This will first search for an LDAP user as described above in Section
"Search authentication". Once an LDAP user is found, `{USER}` in the
search expression is replaced by its distinguished name. For example,
the DN could be `uid=fermi,dc=arangodb,dc=com` and thus with the above
search expression the actual search expression would end up being

    (&(objectClass=groupOfUniqueNames)(uniqueMember=uid=fermi,dc=arangodb,dc=com}))

The list of results of that search would be the list of roles given by
the values of the `dn` attributes of the found role objects.
The actual permissions in ArangoDB for the user will
be the combined permissions of these roles. The database and collection
permissions for the roles are configured directly in ArangoDB as above.


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

  - `--ldap.roles-transformation` can be used to sepcify a regular
    expression and replacement text as `/re/text/`. This regular
    expression is applied to the role name found.

  - `--ldap.superuser-role` can be used to specify the role associated
    with the superuser. Any user belonging to this role gains superuser
    status. This role is checked before applying any regular expression.

Example:

    --ldap.enabled true --ldap.server=ldap.arangodb.com \
    --ldap.roles-attribute-name=role \
    --ldap.prefix=uid= \
    --ldap.suffix=,dc=arangodb,dc=com
    --ldap.roles-include "^arangodb" 

will only consider roles that start with `arangodb`.

    --ldap.enabled true --ldap.server=ldap.arangodb.com \
    --ldap.roles-attribute-name=role \
    --ldap.prefix=uid= \
    --ldap.suffix=,dc=arangodb,dc=com
    --ldap.roles-exclude=disabled

will only consider roles that do contain the word `disabled`.

    --ldap.enabled true --ldap.server=ldap.arangodb.com \
    --ldap.roles-attribute-name=role \
    --ldap.prefix=uid= \
    --ldap.suffix=,dc=arangodb,dc=com
    --ldap.superuser-role "arangodb-admin" 

anyone belonging to the group "arangodb-admin" will become a superuser.


#### LDAP URLs

One can specify the values of multiple LDAP related configuration
options by specifying a single LDAP URL. Here is an example:

    --ldap.url ldap://ldap.arangodb.com:1234/dc=arangodb,dc=com?uid?sub

as one option has the combined effect of setting

    --ldap.server=ldap.arangodb.com \
    --ldap.port=1234 \
    --ldap.basedn=dc=arangodb,dc=com \
    --ldap.searchAttribute=uid \
    --ldap.searchScope=sub

That is, the LDAP URL consists of the ldap *server* and *port*, a *basedn*, a
*search attribute* and a *scope* which can be one of *base*, *one* or
*sub*.


### TLS options

An encrypted connection to the LDAP server can be established with 
`--ldap.tls true` under UNIX and GNU/Linux platforms.

All following options are not available under Windows.

    --ldap.tls

The default is `false`. With `true` a tls connection is established.

    --ldap.tls-version

The default is `1.2`. Available versions are `1.0`, `1.1` and `1.2`.

    --ldap.tls-cert-check-strategy

The default is `hard`. Available strategies are `never`, `hard`,
`demand`, `allow` and `try`.

    --ldap.tls-cacert-file

A file path to one or more (concatenated) certificate authority
certificates in pem format.  As default no file path is configured.

Following option has no effect / does not work under macOS.

    --ldap.tls-cacert-dir

A directory path to certificate authority certificates in
[c_rehash](https://www.openssl.org/docs/man1.0.2/apps/c_rehash.html)
format.  As default no directory path is configured.

