## my environment running ArangoDB
I'm using the latest ArangoDB of the respective release series:
- [ ] 2.8
- [ ] 3.0
- [ ] 3.1
- [ ] 3.2
- [ ] self-compiled devel branch

Mode:
- [ ] Cluster
- [ ] Single-Server

Storage-Engine:
- [ ] mmfiles
- [ ] rocksdb

On this operating system:
- [ ] DCOS on
  - [ ] AWS
  - [ ] Azure
  - [ ] own infrastructure
- [ ] Linux 
  - [ ] Debian .deb
  - [ ] Ubuntu .deb
  - [ ] SUSE   .rpm
  - [ ] RedHat .rpm
  - [ ] Fedora .rpm
  - [ ] Gentoo
  - [ ] docker - official docker library
  - [ ] other:
- [ ] Windows, version: 
- [ ] MacOS, version:


### this is an AQL-related issue:
[ ] I'm using graph features

I'm issuing AQL via:
- [ ] web interface with this browser:     running on this OS:
- [ ] arangosh
- [ ] this Driver:

I've run `db._explain("<my aql query>")` and it didn't shed more light on this.
The AQL query in question is:

The issue can be reproduced using this dataset:

Please provide a way to create the dataset to run the above query on; either by a gist with an arangodump, or `db.collection.save({my: "values"}) statements. If it can be reproduced with one of the ArangoDB example datasets, it's a plus.

### Foxx


### this is a web interface-related issue:
I'm using the web interface with this browser:     running on this OS:
- [ ] authentication is enabled?
- [ ] using the cluster?
- [ ] _system database?

These are the steps to reproduce:
1) open the browser on http://127.0.0.1:8529
2) log in as ...
3) use database [ ] `_system` [ ] other: 
4) click to ...
...

The following problem occurs: [Screenshot?] 
Instead I would be expecting: 


### this is an installation-related issue:
Describe which steps you carried out, what you expected to happen and what actually happened.
