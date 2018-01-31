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

Once the user is authenticated, there are now three methods for
authorization: (a) "roles attribute", (b) "roles search", and
(c) "database permissions attribute". Note that (c) is rather limited
and not recommended for production use.

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

Method (c) is a simplified one and does not use the indirection via
roles. Rather, the access rights for a user are directly configured
in the LDAP server in an attribute of the user object. Therefore, the
access control is coarser and only allows to configure rights on the
database level and not for individual collections. Method (c) is not
recommended for a production setting and is probably only useful in a
testing and development situation. See Section "Database permissions
attribute" below for details on Method (c) and for the associated
configuration options.


Fundamental options to find and connect to the LDAP server
----------------------------------------------------------

The fundamental options for specifying how to access the LDAP server are
the following:

  - `--ldap.enabled`: this is a boolean option which must be set to
    `true` to activate the LDAP feature
  - `--ldap.server`: is a string specifying the host name or IP address
    of the LDAP server
  - `--ldap.port`: is an integer specifying the port the LDAP server is
    running on, the default is *389*
  - `--ldap.binddn` and `--ldap.bindpasswd` are distinguished name and
    password for a read-only LDAP user to which ArangoDB can bind to
    search the LDAP server. Note that it is necessary to configure these
    for both the "simple" and "search" authentication methods, since
    even in the "simple" method, ArangoDB occasionally has to refresh
    the authorization information from the LDAP server
    even if the user session persists and no new authentication is
    needed!

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

**not yet finished**

Example with anonymous auth:

    --ldap.enabled true --ldap.server ldap.company.com \
    --ldap.basedn dc=company,dc=com \
    --ldap.permissions-attribute-name arangodbPermissions

With this configuration ArangoDB binds anonymously to the LDAP server
and searches for the user.  If the user is found a authentication is
done with the users DN and password and then database permissions are
fetched.

Example with DN and password:

    --ldap.enabled true --ldap.server ldap.company.com \
    --ldap.basedn dc=company,dc=com \
    --ldap.binddn cn=admin,dc=company,dc=com \
    --ldap.bindpasswd admin \
    --ldap.permissions-attribute-name arangodbPermissions

With this configuration ArangoDB binds with `--ldap.bindn` and
`--ldap.bindpasswd` to the LDAP server and searches for the user. If
the user is found a authentication is done with the users DN and
password and then database permissions are fetched.


#### Database attribute

**not yet finished**

In this mode, an LDAP attribute of the user is used to specify the
access levels within LDAP. The database/collection access levels in
ArangoDB are not used.

`--ldap.permissions-attribute-name` has the format
*database-name=(&#42;|rw|none)[,database-name=(&#42;|rw|none)]*.

Example:

    --ldap.enabled true --ldap.server ldap.company.com \
    --ldap.permissions-attribute-name arangodbPermissions \
    --ldap.prefix uid= \
    --ldap.suffix ,dc=company,dc=com

`--ldap.prefix` and `--ldap.suffix` build the distinguished name
(DN). ArangoDB tries to authenticate with *prefix* + *ArangoDB
username* + *suffix* against the LDAP server and searches for the
database permissions.

    dn: uid=fermi,dc=example,dc=com
    arangodbPermissions: foo=none,bar=rw

This will give *Administrate* access to *bar* and *No Acess* to *foo*.
Note that this methods only allows to specify database access levels,
not collection access levels.

#### Roles

**not yet finished**

In this mode, an LDAP attribute of the user is used to specify one or
more roles for that users. The database/collection access levels for
these roles defined in ArangoDB are then used.

Example:

    --ldap.enabled true --ldap.server ldap.company.com \
    --ldap.roles-attribute-name groupMembership \
    --ldap.prefix uid= \
    --ldap.suffix ,dc=company,dc=com

`--ldap.prefix` and `--ldap.suffix` build the distinguished name
(DN). ArangoDB trys to authenticate with *prefix* + *ArangoDB
username* + *suffix* against the ldap server and searches for the
roles in the attribute `groupMembership`.

    dn: uid=fermi,dc=example,dc=com
    groupMembership: project-a
    groupMembership: project-b

This will give the combined permissions of the roles `project-a` and
`project-b` to the user.

#### Roles transformations and filters

**not yet finished**

`--ldap.roles-include` can be used to specify a regular expression
that is used to filter roles. Only roles that match the regular
expression are used.

`--ldap.roles-exclude` can be used to specify a regular expression
that is used to filter roles. Only roles that do not match the regular
expression are used.

`--ldap.roles-transformation` can be used to sepcify a regular
expression and replacement text as `/re/text/`. This regular
expression is applied to the role name found.

`--ldap.superuser-role` can be used to specify the role associated
with the superuser. Any user belonging to this role gains superuser
status. This role is checked before applying any regular expression.

Example:

    --ldap.enabled true --ldap.server ldap.company.com \
    --ldap.roles-attribute-name groupMembership \
    --ldap.prefix uid= \
    --ldap.suffix ,dc=company,dc=com
    --ldap.roles-include "^arangodb" 

will only consider roles that start with `arangodb`.

    --ldap.enabled true --ldap.server ldap.company.com \
    --ldap.roles-attribute-name groupMembership \
    --ldap.prefix uid= \
    --ldap.suffix ,dc=company,dc=com
    --ldap.roles-exclude "disabled" 

will only consider roles that do contain the word `disabled`.

    --ldap.enabled true --ldap.server ldap.company.com \
    --ldap.roles-attribute-name groupMembership \
    --ldap.prefix uid= \
    --ldap.suffix ,dc=company,dc=com
    --ldap.superuser-role "arangodb-admin" 

anyone belonging to the group "arangodb-admin" will become a superuser.

#### Roles search

**not yet finished**

    --ldap.roles-search search-expression

Instead of specifying a roles attribute it is possible to use a search
when using *bind+search*. In this case the *search-expression* must be
an ldap search string. Any `{USER}` is replaced by the `dn` of the
user.

Example:

    --ldap.enabled true --ldap.server ldap.company.com \
    --ldap.basedn dc=company,dc=com \
    --ldap.binddn cn=admin,dc=company,dc=com \
    --ldap.bindpasswd admin \
    --ldap.roles-search '(&(objectClass=groupOfUniqueNames)(uniqueMember={USER}))'

### Additional options

**not yet finished**

    --ldap.search-filter "objectClass=*"

Restrict the search to specific object classes. The default is
`objectClass=*`.

    --ldap.search-attribute "uid"

`--ldap.search-attribute` specifies which attribute to compare with
the *username*. The default is `uid`.

    --ldap.search-scope sub

`--ldap.search-scope specifies in which scope to search for a
user. Valid are one of *base*, *one* or *sub*. The default is *sub*.

#### ldap url

**not yet finished**

    --ldap.url ldap://ldap.server.com:1234/dc=example,dc=com?uid?sub

The ldap url consists of the ldap *server* and *port*, a *basedn*, a
*search attribute* and a *scope* which can be one of *base*, *one* or
*sub*.

### TLS options

**not yet finished**

A encrypted connection can be established with `--ldap.tls true` under
UNIX and GNU/Linux platforms.

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

