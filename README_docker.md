# Dockerfile

The Docker image is created on top of `ubuntu:18.04` & includes the development & runtime prerequisites.

# Requirements

 - The project should be cloned from Github: 
 `git clone https://github.com/arangodb/arangodb.git`
 - Docker version 18.06.1-ce
 - Docker compose version 1.22.0
  
# Quick Start

- Copy `Dockerfile` & `docker-compose.yml` to `{project-root-folder}/`.
- Open a terminal session to that folder
- Run `docker-compose -p arango-db build`
- Run `docker-compose -p arango-db up -d`
- Run `docker exec arango-db bash`
- At this point you are inside the docker container, in the root folder of the project. From there, you can run the commands as usual:
	- `mkdir -p build && cd build/` To create build dir and go to there
	- `cmake ..`  to generate the build environment
	- `make` to compile the apps
	- to run the arangodb
	   `build/bin/arangod -c etc/relative/arangod.conf --server.endpoint tcp://0.0.0.0:8529 /tmp/database-dir`
	   `build/bin/arangod -c etc/relative/arangod.conf data`
	
- When you finish working with the container, type `exit`
- Run `docker-compose -p arango-db down` to stop the service.

# Work with the Docker Container

Copy this deliverable (`Dockerfile` & `docker-compose.yml`) to `{project-root-folder}/`

## Build the image

In `{project-root-folder}/` folder, run:

```bash
docker-compose -p arango-db build
```

This instruction will create a Docker image in your machine called `arango-db:latest`

## Run the container

In `{project-root-folder}/` folder, run:

```bash
docker-compose -p arango-db up -d
```

Parameter `-d` makes the container run in detached mode.
This command will create a running container in detached mode called `arango-db`.
You can check the containers running with `docker ps`

## Get a container session

In `{project-root-folder}/` folder, run:

```bash
docker exec arango-db bash
```

## docker-compose.yml

The docker-compose.yml file contains a single service: `arango-db`.
We will use this service to build the ArangoDB sources from our local environment, so we mount root project dir `.` to the a `/app` folder:

```yaml
    volumes:
      - .:/app:Z
```

### More
Please refer to [Readme for maintiainers](README_maintainers.md) and [Compiling on Debian](https://docs.arangodb.com/3.3/Cookbook/Compiling/Debian.html) for more details on the building and running the app.
