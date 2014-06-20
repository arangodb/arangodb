Deprecated Features
-------------------

This file lists all features that have been deprecated in ArangoDB
or are known to become deprecated in a future version of ArangoDB.
Deprecated features will likely be removed in upcoming versions of
ArangoDB and shouldn't be used if possible.

Scheduled removal dates or releases will be listed here. Possible 
migrations will also be detailed here.

## 2.2

* Foxx: The usage of `controller.collection` is no longer suggested, it will be deprecated in the next version. Please use `appContext.collection` instead.
* Foxx: The usage of `apps` in manifest files is no longer possible, please use `controllers` instead.

## 2.3

* Foxx: `controller.collection` is now deprecated, it will raise a warning if you use it. To suppress the warning, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `appContext.collection` instead.

## 2.4

* Foxx: `controller.collection` is no longer available by default. If you still want to use it, please start `arangod` with the option `--server.default-api-compatibility 20200`. Please use `appContext.collection` instead.

## 2.5

* Foxx: `controller.collection` has been removed entirely. Please use `appContext.collection` instead.
