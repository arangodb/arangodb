Foxx configuration
==================

Foxx services can define configuration parameters to make them more re-usable.

The `configuration` object maps names to configuration parameters:

* The key is the name under whicht the parameter will be available
  on the [service context's](Context.md) `configuration` property.

* The value is a parameter definition.

The parameter definition can have the following properties:

* **description**: `string`

  Human readable description of the parameter.

* **type**: `string` (Default: `"string"`)

  Type of the configuration parameter. Supported values are:

  * `"integer"` or `"int"`:
    any finite integer number.

  * `"boolean"` or `"bool"`:
    the values `true` or `false`.

  * `"number"`:
    any finite decimal or integer number.

  * `"string"`:
    any string value.

  * `"json"`:
    any well-formed JSON value.

  * `"password"`:
    like *string* but will be displayed as a masked input field in the web frontend.

* **default**: `any`

  Default value of the configuration parameter.

* **required**: (Default: `true`)

  Whether the parameter is required.

If the configuration has parameters that do not specify a default value, you need to configure the service before it becomes active. In the meantime a fallback servicelication will be mounted that responds to all requests with a HTTP 500 status code indicating a server-side error.

The configuration parameters of a mounted service can be adjusted from the web interface by clicking the *Configuration* button in the service details.

<!-- TODO (Link to admin docs) -->

**Examples**

```json
"configuration": {
  "currency": {
    "description": "Currency symbol to use for prices in the shop.",
    "default": "$",
    "type": "string"
  },
  "secretKey": {
    "description": "Secret key to use for signing session tokens.",
    "type": "password"
  }
}
```
