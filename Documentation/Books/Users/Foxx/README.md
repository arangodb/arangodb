Foxx
====

Foxx is a framework that allows you to write data-centric microservices.
It is executed directly inside of ArangoDB and grants you raw access to your data.
With the framework you can embed complex queries and data-focused business logic into your database.

Using Foxx will give you several benefits above writing the code in separate applications.
First of all it yields performance benefits, the data analysis and modification is executed as close to the data as possible.
Also it requires one network hop less as the Foxx has direct access to the data and does not have to query a database located on a different server.
Second your application is completely free of any database related information like queries, it just requests an HTTP-endpoint that gives the data in the expected format.
This also allows to reuse this endpoint in different applications without copying the queries.
Third Foxx adds another level of security to your application, you can manage the access rights on any level you like: application, collection, document or even attribute.
Finally Foxx is automatically scaled alongside your database: if the number of ArangoDB servers grows so does your Foxx servers, both have their bottleneck at the same point.

In the recent movements of software development many companies of all sizes talk about
moving away from large monolithic applications and switch over to a microservice based software architecture.
Because microservices yield many benefits in terms of scaling and zero-downtime.

During the design of Foxx we followed the concepts and ideas of microservices.
Our baseline is the definition by Martin Fowler and James Lewis given in their [microservice article](http://martinfowler.com/articles/microservices.html).
The following table lists a mapping of microservice concepts and how they can be realized within Foxx:

<table>
  <thead>
    <tr>
      <th>microservice</th>
      <th>Foxx</th>
      <th>description</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>Few lines of Code</td>
      <td>Little Boilerplate</td>
      <td>Most of the boilerplate is already handled by the Foxx environment. You can focus on your business logic.</td>
    </tr>
    <tr>
      <td>HTTP API</td>
      <td>Controller</td>
      <td>The Foxx controller allows to easily define HTTP API routes.</td>
    </tr>
    <tr>
      <td>Independently Deployable</td>
      <td>Foxx Manager</td>
      <td>Foxxes run in ArangoDB Coordinators. These are independent from one another. Foxxes can be installed on an arbitrary number of these Coordinators.</td>
    </tr>
    <tr>
      <td>Access other Microservices</td>
      <td>Requests</td>
      <td>The Foxx request module offers a simple functionality to contact any other API via HTTP.</td>
    </tr>
    <tr>
      <td>Scalability</td>
      <td>Sharding</td>
      <td>The dataset managed by a Foxx micro-service might be too large for a single server. The underlying ArangoDB infrastructure can handle this for you.</td>
    </tr>
    <tr>
      <td>Persistence</td>
      <td>Repositories</td>
      <td>Foxx is part of a multi-model database. It has all features to persist data of diverse formats (Graphs, Documents) built-in.</td>
    </tr>
    <tr>
      <td>Reuse Services</td>
      <td>Arango Store</td>
      <td>The Arango Store contains several Foxxes. Many of them are general-use services that are required in many applications. You can just reuse the reviewed and tested code of other Foxx users.</td>
    </tr>
    <tr>
      <td>Automated Deployment</td>
      <td>Foxx Manager</td>
      <td>The Foxx manager tool allows to trigger installation of new applications. Also there is a native system HTTP API offering this feature. You can use either one as a step in your build automation tool.</td>
    </tr>
    <tr>
      <td>Design for failure</td>
      <td>Coordinators</td>
      <td>ArangoDBs cluster setup is designed for failure. You can make use of this feature in your Foxxes.</td>
    </tr>
    <tr>
      <td>Different Languages</td>
      <td>Connect to other APIs</td>
      <td>Foxxes are only implemented in JavaScript. However not your entire application has to be written in Foxx. You can connect via HTTP to services in other languages.</td>
    </tr>
    <tr>
      <td>Different Databases</td>
      <td>Multi-Model</td>
      <td>In most setups you need different database technologies because you have several data formats. ArangoDB is a multi-model database that serves many formats. However if you still need another database you can connect to it via HTTP.</td>
    </tr>
    <tr>
      <td>Asynchronous Calls</td>
      <td>Synchronous with multiple contexts</td>
      <td>Foxx only allows synchronous execution. However it is executed in a multi-threaded fashion, hence blocking one thread is not a problem. Also background tasks are available for long-running tasks that should be executed outside of the request-response cycle.</td>
    </tr>
    <tr>
      <td>Security</td>
      <td>Authentication and Session</td>
      <td>Foxx offers internal APIs to handle authentication and manage session data. It is just a handful lines of code and your service is secured.</td>
    </tr>
    <tr>
      <td>Protected API</td>
      <td>Libraries</td>
      <td>Sometimes you have sensitive data that is not allowed to ever leave your database. Using libraries in Foxx you can write internal APIs that can only be accessed from the same server and are not exposed to the outside world. Perfect to keep your data inside but still using the paradigm of a microservice for it.</td>
    </tr>
    <tr>
      <td>How To</td>
      <td>Recipes</td>
      <td>In ArangoDBs cookbook there are several recipes on how to design a microservice based application with the help of Foxx.</td>
    </tr>
    <tr>
      <td>Maintenance UI</td>
      <td>Built-in Web server</td>
      <td>ArangoDB has a built-in Web server, allowing Foxxes to ship a Webpage as their UI. So you can attach a maintenance UI directly to your microservice and have it deployed with it in the same step.</td>
    </tr>
    <tr>
      <td>Easy Setup</td>
      <td>ArangoDB on Mesos</td>
      <td>ArangoDB perfectly integrates with the [Mesos project](http://mesos.apache.org/), a distributed kernel written for data centers. You can set up and deploy a Mesos instance running ArangoDB and having your microservice on top of it with just a single command.</td>
    </tr>
    <tr>
      <td>Maintenance</td>
      <td>Marathon</td>
      <td>If you are running ArangoDB on top of Mesos you can use [Marathon](https://mesosphere.github.io/marathon/) to keep all your instances alive and scale up or down with just a few clicks. Do not bother with long configuration, that is all handled by the underlying systems.</td>
    </tr>
  </tbody>
</table>

As you can see most concepts of microservices are natively available in Foxx plus several additions that ease your development
process.
Thats why we like to call Foxx a "microservice framework": It allows you to write and deploy data-centric microservices 
on top of ArangoDB.

If this short introduction has piqued your interest you can find more details in the sections below.
We recommend to start with Foxx in a nutshell which gives a really quick introduction in how to
create and use your first Foxx.
All other sections will give very detailed informations about the features available and a peak behind the curtains.

Foxx in a nutshell
------------------

This chapter is for the impatient and for the ones that just want to get a quick overview
of what Foxx is, how it works and how to get it up and running in short time.
It is tutorial that should be solvable in 10 minutes or less and will guide you to your first
application giving a few insights on the way.

[Read More](Nutshell/README.md)

Install a Foxx Application
--------------------------

You would like to get some inspiration from running Applications?
Or you already have an Application that does what you need?
This chapter will guide you to get an Application up and running.

[Read More](Install/README.md)

Develop your own Foxx
---------------------

Ready to get to bare metal?
In this chapter you will get to know the details on Foxx development.
It will explain how to use the development mode and which debugging mechanisms are at hand.
It introduces the tips and tricks on how to build your own API.
And presents all libraries that are available from a Foxx Application.

[Read More](Develop/README.md)


Running Foxx Applications in production
---------------------------------------

This chapter is dedicated to all the possibilities on how to run a Foxx Application in production.
It will start with how to leverage it from development to production.
We will continue with migration of the Application to a newer version.

[Read More](Production/README.md)

Foxx in a cluster
-----------------

Ready to scale out?
Want to setup your micro-service based application?
This chapter will explain what has to be changed to run a Foxx application within a cluster setup.
It will also explain how to move your application from a single server to an unlimited amount of servers.

[Read More](Cluster/README.md)
