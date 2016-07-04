!SECTION Single instance deployment

The latest official builds of ArangoDB for all supported operating systems may be obtained from https://www.arangodb.com/download/.

!SUBSECTION Linux remarks

Besides the official images which are provided for the most popular linux distributions there are also a variety of unofficial images provided by the community. We are tracking most of the community contributions (including new or updated images) in our newsletter:

https://www.arangodb.com/category/newsletter/

!SUBSECTION Windows remarks

Please note that ArangoDB will only work on 64bit.

!SUBSECTION Docker

The simplest way to deploy ArangoDB is using [Docker](https://docker.io/). To get a general understanding of Docker have a look at [their excellent documentation](https://docs.docker.com/).

!SUBSUBSECTION Authentication

To start the official Docker container you will have to decide on an authentication method. Otherwise the container won't start.

Provide one of the arguments to Docker as an environment variable.

There are three options:

1. ARANGO_NO_AUTH=1

   Disable authentication completely. Useful for local testing or for operating in a trusted network (without a public interface).
        
2. ARANGO_ROOT_PASSWORD=password

   Start ArangoDB with the given password for root
        
3. ARANGO_RANDOM_ROOT_PASSWORD=1

   Let ArangoDB generate a random root password
        
To get going quickly:

`docker run -e ARANGO_RANDOM_ROOT_PASSWORD=1 arangodb/arangodb`

For an in depth guide about Docker and ArangoDB please check the official documentation: https://hub.docker.com/r/arangodb/arangodb/ . Note that we are using the image `arangodb/arangodb` here which is always the most current one. There is also the "official" one called `arangodb` whose documentation is here: https://hub.docker.com/_/arangodb/
