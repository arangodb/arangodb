LDAP
====

__This feature is available in the Enterprise Edition.__

The basic options are `--ldap.enabled`, `--ldap.tls`, `--ldap.port`, `--ldap.server` and `--ldap.permissions-attribute-name`.

`--ldap.server` and `--ldap.port` can be replace by `--ldap.url`.

The default for `--ldap.port` is *389*.

`--ldap.permissions-attribute-name` has the format *databse-name=(&#42;|rw|none)[,database-name=(&#42;|rw|none)]*.

There are two modes of operation: *simple auth* and *bind+search*.

### simple auth

ArangoDB connects to the ldap server and authenticates with the username and password provided by the 
api authentication request and searches for the database permissions using the attribute name 
provided by `--ldap.permissions-attribute-name`.

Example:

    --ldap.enabled true --ldap.server ldap.company.com \
    --ldap.permissions-attribute-name arangodbPermissions \
    --ldap.prefix uid= --ldap.suffix ,dc=company,dc=com

`--ldap.prefix` and `--ldap.suffix` build the distinguished name (DN). ArangoDB trys to authenticate
with *prefix* + *ArangoDB username* + *suffix* against the ldap server and searches for the database permissions.

### bind+search

Example with anonymous auth:

    --ldap.enabled true --ldap.server ldap.company.com \
    --ldap.basedn dc=company,dc=com \
    --ldap.permissions-attribute-name arangodbPermissions

With this configuration ArangoDB binds anonymously to the ldap server and searches for the user.
If the user is found a authentication is done with the users DN and password and then database permissions are fetched.

Example with DN and password:

    --ldap.enabled true --ldap.server ldap.company.com \
    --ldap.basedn dc=company,dc=com \
    --ldap.permissions-attribute-name arangodbPermissions
    --ldap.binddn cn=admin,dc=company,dc=com --ldap.bindpasswd admin

With this configuration ArangoDB binds with `--ldap.bindn` and `--ldap.bindpasswd` to the ldap server and searches for the user.
If the user is found a authentication is done with the users DN and password and then database permissions are fetched.

#### additional options


    --ldap.search-filter "objectClass=*"

Restrict the search to specific object classes. The default is `objectClass=*`.

    --ldap.search-attribute "uid"

`--ldap.search-attribute` specifies which attribute to compare with the *username*. The default is `uid`.

    --ldap.search-scope sub

`--ldap.search-scope specifies in which scope to search for a user. Valid are one of *base*, *one* or *sub*. The default is *sub*.

### ldap url

    --ldap.url ldap://ldap.server.com:1234/dc=example,dc=com?uid?sub

The ldap url consists of the ldap *server* and *port*, a *basedn*, a *search attribute* and a *scope* which can be one of *base*, *one* or *sub*.

### TLS options

A encrypted connection can be established with `--ldap.tls true` under UNIX and GNU/Linux platforms.

All following options are not available under Windows.

    --ldap.tls

The default is `false`. With `true` a tls connection is established.

    --ldap.tls-version

The default is `1.2`. Available versions are `1.0`, `1.1` and `1.2`.

    --ldap.tls-cert-check-strategy

The default is `hard`. Available strategies are `never`, `hard`, `demand`, `allow` and `try`.

    --ldap.tls-cacert-file

A file path to one or more (concatenated) certificate authority certificates in pem format.
As default no file path is configured.

Following option has no effect / does not work under macOS.

    --ldap.tls-cacert-dir

A directory path to certificate authority certificates in [c_rehash](https://www.openssl.org/docs/man1.0.2/apps/c_rehash.html) format.
As default no directory path is configured.
