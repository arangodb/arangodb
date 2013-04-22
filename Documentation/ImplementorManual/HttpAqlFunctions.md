HTTP Interface for AQL User Functions Management {#HttpAqlFunctions}
====================================================================

@NAVIGATE_HttpAqlFunctions
@EMBEDTOC{HttpAqlFunctionsTOC}

AQL User Functions Management {#HttpAqlFunctionsIntro}
======================================================

This is an introduction to ArangoDB's Http interface for managing AQL
user functions. AQL user functions are a means to extend the functionality
of ArangoDB's query language (AQL) with user-defined Javascript code.
 
For an overview of how AQL user functions work, please refer to @ref
ExtendingAql.

The Http interface provides an API for adding, deleting, and listing
previously registered AQL user functions.

All user functions managed through this interface will be stored in the 
system collection `_aqlfunctions`. Documents in this collection should not
be accessed directly, but only via the dedicated interfaces.

@anchor HttpAqlFunctionsSave
@copydetails JSF_POST_api_aqlfunction

@CLEARPAGE
@anchor HttpAqlFunctionsRemove
@copydetails JSF_DELETE_api_aqlfunction

@CLEARPAGE
@anchor HttpAqlFunctionsList
@copydetails JSF_GET_api_aqlfunction

