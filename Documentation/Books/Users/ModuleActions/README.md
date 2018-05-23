Module "actions"
================

The action module provides the infrastructure for defining HTTP actions.

Basics
------
### Error message
<!-- js/server/modules/org/arangodb/actions.js -->
@startDocuBlock actionsGetErrorMessage

Standard HTTP Result Generators
-------------------------------

@startDocuBlock actionsDefineHttp

### Result ok
<!-- js/server/modules/org/arangodb/actions.js -->
@startDocuBlock actionsResultOk

### Result bad
<!-- js/server/modules/org/arangodb/actions.js -->
@startDocuBlock actionsResultBad

### Result not found
<!-- js/server/modules/org/arangodb/actions.js -->
@startDocuBlock actionsResultNotFound

### Result unsupported
<!-- js/server/modules/org/arangodb/actions.js -->
@startDocuBlock actionsResultUnsupported

### Result error
<!-- js/server/modules/org/arangodb/actions.js -->
@startDocuBlock actionsResultError

### Result not Implemented
<!-- js/server/modules/org/arangodb/actions.js -->
@startDocuBlock actionsResultNotImplemented

### Result permanent redirect
<!-- js/server/modules/org/arangodb/actions.js -->
@startDocuBlock actionsResultPermanentRedirect

### Result temporary redirect
<!-- js/server/modules/org/arangodb/actions.js -->
@startDocuBlock actionsResultTemporaryRedirect

ArangoDB Result Generators
--------------------------

### Collection not found
<!-- js/server/modules/org/arangodb/actions.js -->
@startDocuBlock actionsCollectionNotFound

### Index not found
<!-- js/server/modules/org/arangodb/actions.js -->
@startDocuBlock actionsIndexNotFound

### Result exception
<!-- js/server/modules/org/arangodb/actions.js -->
@startDocuBlock actionsResultException