# Pull Request Guidelines

Pull requests are an essential collaborative tool for modern software development. 

The below list is intended to help you figure out whether your code is ready to be reviewed
and merged into ArangoDB. The overarching goal is to:

- Reduce the amount of *recurring* defects
- Reduce the impact to the other developer’s time and energy spent on defects in your code
- Increase the overall autonomy and productivity of individual developers

## Acceptance Checklist

*below list is not exhaustive, think thoroughly whether the provided information is sufficient.*
*remove options that do not apply*

### Scope & Purpose

*(Can you describe what functional change your PR is trying to effect?)*

- [ ] This is a **Bug-Fix** for a released Version ?
- [ ] Does this create **new functionality** (i.e. a new feature / new option in an existing API) ?
- [ ] Have you *manually tested* the behavior you are affecting in this PR ?

#### Related Information

*(Please reference tickets / specification etc )*

- [ ] There is a GitHub Issue reported by a Community User: 
- [ ] There is an internal planning ticket: 
- [ ] There is a *JIRA Ticket number* (In case a customer was affected / involved): 
- [ ] There is a design document: 

### Testing & Verification

*(Please pick either of the following options)*

This change is a trivial rework / code cleanup without any test coverage.

*(or)*

This change is already covered by existing tests, such as *(please describe tests)*.

*(or)*

This change added tests and can be verified as follows:

- [ ] Added **Regression Tests** (Only for bug-fixes) 
- [ ] Added new C++ *Unit Tests* (Either GoogleTest or Catch-Test)
   - Did you add tests for a new RestHandler subclass ?
   - Did you add new mocks of underlying code layers to be able to verify your functionality ? 
   - ...
- [ ] Added new *integration tests* (i.e. in shell_server / shell_server_aql)
- [ ] Added new *resilience tests* (only if the feature is impacted by failovers)
- [ ] There are tests in one of the external test repos (i.e. node-resilience tests, chaos tests)
- [ ] I ensured this code runs with ASan / TSan or other static verification tools

*(Include link to Jenkins run etc)*

> Think about whether the new code you added is modular enough to be
> easily testable by unit tests written with GTest / Catch. It is not good if your feature is so interconnected
> that it prevents other people from writing their own unit gests. It should be possible
> to use your code in future without extensively mocking your classes.
> A bad example that required some extensive effort would be the storage engine API.

### Documentation

> All new Features should be accompanied by corresponding documentation. 
> Bugs and features should furthermore be documented in the changelog so that
> developers and users have a concise overview. 

- [ ] Added a *Changelog Entry* 
- [ ] Added entry to *Release Notes* 
- [ ] Added a new section in the *Manual* 
- [ ] Added a new section in the *http API* 
- [ ] Added *Swagger examples* for the http API  

### CLA Note 

Please note that for legal reasons we require you to sign the [Contributor Agreement](https://www.arangodb.com/documents/cla.pdf)
before we can accept your pull requests.
