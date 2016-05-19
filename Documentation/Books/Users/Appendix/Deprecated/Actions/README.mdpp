!CHAPTER ArangoDB's Actions

!SUBSECTION Introduction to User Actions

In some ways the communication layer of the ArangoDB server behaves like a Web
server. Unlike a Web server, it normally responds to HTTP requests by delivering
JSON objects. Remember, documents in the database are just JSON objects. So,
most of the time the HTTP response will contain a JSON document from the
database as body. You can extract the documents stored in the database using
HTTP *GET*. You can store documents using HTTP *POST*.

However, there is something more. You can write small snippets - so called
actions - to extend the database. The idea of actions is that sometimes it is
better to store parts of the business logic within ArangoDB.

The simplest example is the age of a person. Assume you store information about
people in your database. It is an anti-pattern to store the age, because it
changes every now and then. Therefore, you normally store the birthday and let
the client decide what to do with it. However, if you have many different
clients, it might be easier to enrich the person document with the age using
actions once on the server side.

Or, for instance, if you want to apply some statistics to large data-sets and
you cannot easily express this as query. You can define a action instead of
transferring the whole data to the client and do the computation on the client.

Actions are also useful if you want to restrict and filter data according to
some complex permission system.

The ArangoDB server can deliver all kinds of information, JSON being only one
possible format. You can also generate HTML or images. However, a Web server is
normally better suited for the task as it also implements various caching
strategies, language selection, compression and so on. Having said that, there
are still situations where it might be suitable to use the ArangoDB to deliver
HTML pages - static or dynamic. A simple example is the built-in administration
interface. You can access it using any modern browser and there is no need for a
separate Apache or IIS.

In general you will use [Foxx](../../../Foxx/README.md) to easily extend the database with
business logic. Foxx provides an simple to use interface to actions.

The following sections will explain the low-level actions within ArangoDB on
which Foxx is built and show how to define them. The examples start with
delivering static HTML pages - even if this is not the primary use-case for
actions. The later sections will then show you how to code some pieces of your
business logic and return JSON objects.

The interface is loosely modeled after the JavaScript classes for HTTP request
and responses found in node.js and the middleware/routing aspects of connect.js
and express.js.

Note that unlike node.js, ArangoDB is multi-threaded and there is no easy way to
share state between queries inside the JavaScript engine. If such state
information is required, you need to use the database itself.