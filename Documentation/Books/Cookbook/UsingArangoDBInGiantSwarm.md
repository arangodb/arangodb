# ArangoDB in the Giant Swarm using Docker containers

## Problem 

I want to use ArangoDB in the Giant Swarm with Docker containers.

## Solution

Giant Swarm allows you to describe and deploy your application by providing a simple JSON
description. The
[current weather app](http://docs.giantswarm.io/guides/your-first-application/nodejs/) is a good
example on how to install an application which uses two components, namely `node` and `redis`.

My colleague Max has written a guesser game with various front-ends and ArangoDB as
backend. In order to get the feeling of being part of the Giant Swarm, I have started to
set up this game in the [swarm](https://giantswarm.io).

### First Steps

The guesser game consists of a front-end written as express application in node and a
storage back-end using ArangoDB and a small API developed with Foxx.

The front-end application is available as image

    arangodb/example-guesser

and the ArangoDB back-end with the Foxx API as

    arangodb/example-guesser-db

The dockerfiles used to create the images are available from github

    https://github.com/arangodb/guesser

### Set up the Swarm

Set up your swarm environment as described in the documentation. Create a configuration
file for the swarm called `arangodb.json` and fire up the application

    {
      "app_name": "guesser",
      "services": [
        {
          "service_name": "guesser-game",
          "components": [
            {
              "component_name": "guesser-front-end",
              "image": "arangodb/example-guesser",
              "ports": [ 8000 ],
              "dependencies": [
                { "name": "guesser-back-end", "port": 8529 }
              ],
              "domains": { "guesser.gigantic.io": 8000 }
            },
            {
              "component_name": "guesser-back-end",
              "image": "arangodb/example-guesser-db",
              "ports": [ 8529 ]
            }
          ]
        }
      ]
    }

This defines an application `guesser` with a single service `guesser-game`. This
service has two components `guesser-front-end` and `guesser-back-end`. The
docker images are downloaded from the standard docker repository.

The line

    "domains": { "guesser.gigantic.io": 8000 }

exposes the internal port 8000 to the external port on port 80 for the host
`guesser.gigantic.io`.

In order to tell Giant Swarm about your application, execute

    unix> swarm create arangodb.json 
    Creating 'arangodb' in the 'fceller/dev' environment...
    App created successfully!    

This will create an application called `guesser`.

    unix> swarm status guesser
    App guesser is down

    service       component          instanceid                            status
    guesser-game  guesser-back-end   5347e718-3d27-4356-b530-b24fc5d1e3f5  down
    guesser-game  guesser-front-end  7cf25b43-13c4-4dd3-9a2b-a1e32c43ae0d  down

We see the two components of our application. Both are currently powered down.

### Startup the Guesser Game

Starting your engines is now one simple command

    unix> swarm start guesser
    Starting application guesser...
    Application guesser is up

Now the application is up
    
    unix> swarm status guesser
    App guesser is up

    service       component          instanceid                            status
    guesser-game  guesser-back-end   5347e718-3d27-4356-b530-b24fc5d1e3f5  up
    guesser-game  guesser-front-end  7cf25b43-13c4-4dd3-9a2b-a1e32c43ae0d  up

Point your browser to

    http://guesser.gigantic.io

and guess an animal.

If you want to check the log files of an instance you can ask the swarm giving it the
instance id. For example, the back-end 

    unix> swarm logs 5347e718-3d27-4356-b530-b24fc5d1e3f5
    2014-12-17 12:34:57.984554 +0000 UTC - systemd - Stopping User guesser-back-end...
    2014-12-17 12:36:28.074673 +0000 UTC - systemd - 5cfe11d6-343e-49bb-8029-06333844401f.service stop-sigterm timed out. Killing.
    2014-12-17 12:36:28.077821 +0000 UTC - systemd - 5cfe11d6-343e-49bb-8029-06333844401f.service: main process exited, code=killed, status=9/KILL
    2014-12-17 12:36:38.213245 +0000 UTC - systemd - Stopped User guesser-back-end.
    2014-12-17 12:36:38.213543 +0000 UTC - systemd - Unit 5cfe11d6-343e-49bb-8029-06333844401f.service entered failed state.
    2014-12-17 12:37:55.074158 +0000 UTC - systemd - Starting User guesser-back-end...
    2014-12-17 12:37:55.208354 +0000 UTC - docker  - Pulling repository arangodb/example-guesser-db
    2014-12-17 12:37:56.995122 +0000 UTC - docker  - Status: Image is up to date for arangodb/example-guesser-db:latest
    2014-12-17 12:37:57.000922 +0000 UTC - systemd - Started User guesser-back-end.
    2014-12-17 12:37:57.707575 +0000 UTC - docker  - --> starting ArangoDB
    2014-12-17 12:37:57.708182 +0000 UTC - docker  - --> waiting for ArangoDB to become ready
    2014-12-17 12:38:28.157338 +0000 UTC - docker  - --> installing guesser game
    2014-12-17 12:38:28.59025 +0000 UTC  - docker  - --> ready for business

and the front-end

    unix> swarm logs 7cf25b43-13c4-4dd3-9a2b-a1e32c43ae0d
    2014-12-17 12:35:10.139684 +0000 UTC - systemd - Stopping User guesser-front-end...
    2014-12-17 12:36:40.32462 +0000 UTC  - systemd - aa7756a4-7a87-4633-bea3-e416d035188b.service stop-sigterm timed out. Killing.
    2014-12-17 12:36:40.327754 +0000 UTC - systemd - aa7756a4-7a87-4633-bea3-e416d035188b.service: main process exited, code=killed, status=9/KILL
    2014-12-17 12:36:50.567911 +0000 UTC - systemd - Stopped User guesser-front-end.
    2014-12-17 12:36:50.568204 +0000 UTC - systemd - Unit aa7756a4-7a87-4633-bea3-e416d035188b.service entered failed state.
    2014-12-17 12:38:04.796129 +0000 UTC - systemd - Starting User guesser-front-end...
    2014-12-17 12:38:04.921273 +0000 UTC - docker  - Pulling repository arangodb/example-guesser
    2014-12-17 12:38:06.459366 +0000 UTC - docker  - Status: Image is up to date for arangodb/example-guesser:latest
    2014-12-17 12:38:06.469988 +0000 UTC - systemd - Started User guesser-front-end.
    2014-12-17 12:38:07.391149 +0000 UTC - docker  - Using DB-Server http://172.17.0.183:8529
    2014-12-17 12:38:07.613982 +0000 UTC - docker  - Guesser app server listening at http://0.0.0.0:8000

### Scaling Up

Your game becomes a success. Well, scaling up the front-end is trivial.

Simply change your configuration file and recreate the application:

    {
      "app_name": "guesser",
      "services": [
        {
          "service_name": "guesser-game",
          "components": [
            {
              "component_name": "guesser-front-end",
              "image": "arangodb/example-guesser",
              "ports": [ 8000 ],
              "dependencies": [
                { "name": "guesser-back-end", "port": 8529 }
              ],
              "domains": { "guesser.gigantic.io": 8000 },
              "scaling_policy": { "min": 2, "max": 2 }
            },
            {
              "component_name": "guesser-back-end",
              "image": "arangodb/example-guesser-db",
              "ports": [ 8529 ]
            }
          ]
        }
      ]
    }

The important line is

    "scaling_policy": { "min": 2, "max": 2 }
    
It tells the swarm to use two front-end containers. In later version of the `swarm` you will be able to change the number of containers in a running application with the command:

    > swarm scaleup guesser/guesser-game/guesser-front-end --count=1
    Scaling up component guesser/guesser-game/guesser-front-end by 1...

We at ArangoDB are hard at work to make scaling up the back-end database equally easy. Stay tuned for new releases in early 2015... 

**Authors**: [Frank Celler](https://github.com/fceller)

**Tags**: #docker, #giantswarm, #howto
