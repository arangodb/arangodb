LDAP
====

__This feature is available in the Enterprise Edition.__

Basics Concepts
---------------

There are two modes of operation: *simple auth* and *bind+search*.

The basic options for specifying how to access the LDAP server are
`--ldap.enabled`, `--ldap.tls`, `--ldap.port`,
`--ldap.server`. `--ldap.server` and `--ldap.port` can be replace by
`--ldap.url`. The default for `--ldap.port` is *389*.

### Simple auth

ArangoDB connects to the ldap server and authenticates with the
username and password provided by the api authentication request.  If
ldap server verifies the the password then the user is authenticated.

If `--ldap.prefix` and/or `--ldap.suffix` is provided then the simple
mode is selected.

In order to authorize the user for one or more databases there are two
modes of operation: *database attribute* or *roles*.

#### Database attribute

In this mode, an ldap sttribute of the user is used to specify the
access levels within ldap. The database/collection access levels in
ArangoDB are not used.

`--ldap.permissions-attribute-name` has the format
*databse-name=(&#42;|rw|none)[,database-name=(&#42;|rw|none)]*.

Example:

    --ldap.enabled true --ldap.server ldap.company.com \
    --ldap.permissions-attribute-name arangodbPermissions \
    --ldap.prefix uid= \
    --ldap.suffix ,dc=company,dc=com

`--ldap.prefix` and `--ldap.suffix` build the distinguished name
(DN). ArangoDB trys to authenticate with *prefix* + *ArangoDB
username* + *suffix* against the ldap server and searches for the
database permissions.

    dn: uid=fermi,dc=example,dc=com
    arangodbPermissions: foo=none,bar=rw

This will give *Administrate* access to *bar* and *No Acess* to *foo*.
Note that this methods only allows to specify database access levels,
not collection access levels.

#### Roles

In this mode, an ldap sttribute of the user is used to specify one or
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

`--ldap.roles-include` can be used to specify a regular expression
that is used to filter roles. Only roles that match the regular
expression are used.

`--ldap.roles-exclude` can be used to specify a regular expression
that is used to filter roles. Only roles that do not match the regular
expression are used.

`--ldap.roles-transformation` can be used to sepcify a regular
expression and replacement text as `/re/text/`. This regular
expression is apply to the role name found.

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

### bind+search

Example with anonymous auth:

    --ldap.enabled true --ldap.server ldap.company.com \
    --ldap.basedn dc=company,dc=com \
    --ldap.permissions-attribute-name arangodbPermissions

With this configuration ArangoDB binds anonymously to the ldap server
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
`--ldap.bindpasswd` to the ldap server and searches for the user.  If
the user is found a authentication is done with the users DN and
password and then database permissions are fetched.

#### Roles search

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

    --ldap.url ldap://ldap.server.com:1234/dc=example,dc=com?uid?sub

The ldap url consists of the ldap *server* and *port*, a *basedn*, a
*search attribute* and a *scope* which can be one of *base*, *one* or
*sub*.

### TLS options

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
