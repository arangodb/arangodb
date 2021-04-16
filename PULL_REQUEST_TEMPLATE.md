### Scope & Purpose

*(Please describe the changes in this PR for reviewers - **mandatory**)*

- [ ] :hankey: Bugfix (requires CHANGELOG entry)
- [ ] :pizza: New feature (requires CHANGELOG entry, feature documentation and release notes)
- [ ] :fire: Performance improvement
- [ ] :hammer: Refactoring/simplification
- [ ] :book: CHANGELOG entry made

#### Backports:

- [ ] No backports required
- [ ] Backports required for: *(Please specify versions and link PRs)*

#### Related Information

*(Please reference tickets / specification / other PRs etc)*

- [ ] Docs PR: 
- [ ] Enterprise PR:
- [ ] GitHub issue / Jira ticket number:
- [ ] Design document: 

### Testing & Verification

*(Please pick either of the following options)*

- [ ] This change is a trivial rework / code cleanup without any test coverage.
- [ ] The behavior in this PR was *manually tested*
- [ ] This change is already covered by existing tests, such as *(please describe tests)*.
- [ ] This PR adds tests that were used to verify all changes:
  - [ ] Added new C++ **Unit tests**
  - [ ] Added new **integration tests** (e.g. in shell_server / shell_server_aql)
  - [ ] Added new **resilience tests** (only if the feature is impacted by failovers)
- [ ] There are tests in an external testing repository:
- [ ] I ensured this code runs with ASan / TSan or other static verification tools

Link to Jenkins PR run:

### Documentation

> All new features should be accompanied by corresponding documentation. 
> Bugs and features should furthermore be documented in the CHANGELOG so that
> support, end users, and other developers have a concise overview. 

- [ ] Added entry to *Release Notes* 
- [ ] Added a new section in the *Manual* 
- [ ] Added a new section in the *HTTP API* 
- [ ] Added *Swagger examples* for the HTTP API  
- [ ] Updated license information in *LICENSES-OTHER-COMPONENTS.md* for 3rd party libraries

### External contributors / CLA Note 

Please note that for legal reasons we require you to sign the [Contributor Agreement](https://www.arangodb.com/documents/cla.pdf)
before we can accept your pull requests.
