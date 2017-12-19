!CHAPTER HTTP Interface for AQL User Functions Management

!SUBSECTION AQL User Functions Management

This is an introduction to ArangoDB's HTTP interface for managing AQL
user functions. AQL user functions are a means to extend the functionality
of ArangoDB's query language (AQL) with user-defined JavaScript code.
 
For an overview of how AQL user functions and their implications, please refer to
the [Extending AQL](../../AQL/Extending/index.html) chapter.

The HTTP interface provides an API for adding, deleting, and listing
previously registered AQL user functions.

All user functions managed through this interface will be stored in the 
system collection *_aqlfunctions*. Documents in this collection should not
be accessed directly, but only via the dedicated interfaces.

<!-- js/actions/api-aqlfunction.js -->
@startDocuBlock JSF_post_api_aqlfunction

<!-- js/actions/api-aqlfunction.js -->
@startDocuBlock JSF_delete_api_aqlfunction

<!-- js/actions/api-aqlfunction.js -->
@startDocuBlock JSF_get_api_aqlfunction
