Modifying a View
================

### Renaming a View

Views, just as collections, can be renamed. View rename is achieved via an API
common to all view types, as follows:

<!-- js/actions/api-view.js -->
@startDocuBlock put_api_view_rename

### Modifying View Properties

Some view types allow run-time modification of internal properties. Which, if
any properties are modifiable is implementation dependant and varies for each
supported view type. Please refer to the proper section of the required view
type for details.

However, in general the format is the following:

Update of All Possible Properties
---------------------------------

All modifiable properties of a view may be set to the specified definition,
(i.e. "make the view exactly like *this*"), via:

<!-- js/actions/api-view.js -->
@startDocuBlock put_api_view_properties

Update of Specific Properties (delta)
-------------------------------------

Specific modifiable properties of a view may be set to the specified values,
(i.e. "change only these properties to the specified values"), via:

<!-- js/actions/api-view.js -->
@startDocuBlock patch_api_view_properties
