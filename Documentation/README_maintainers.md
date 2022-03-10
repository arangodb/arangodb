# ArangoDB Documentation Maintainers manual

- [Swagger integration](#swagger-integration)
  * [RESTQUERYPARAMETERS](#restqueryparameters)
  * [RESTURLPARAMETERS](#resturlparameters)
  * [RESTDESCRIPTION](#restdescription)
  * [RESTRETURNCODES](#restreturncodes)
  * [RESTREPLYBODY](#restreplybody)
  * [RESTHEADER](#restheader)
  * [RESTURLPARAM](#resturlparam)
  * [RESTALLBODYPARAM](#restallbodyparam)
  * [RESTBODYPARAM](#restbodyparam)
  * [RESTSTRUCT](#reststruct)

# Swagger integration

`./utils/generateSwagger.sh` scans the documentation, and generates swagger output.
It scans for all documentationblocks containing `@RESTHEADER`.
It is a prerequisite for integrating these blocks into the gitbook documentation.

Tokens may have curly brackets with comma separated fields. They may optionally be followed by subsequent text
lines with a long descriptions.

Sections group a set of tokens; they don't have parameters.

**Types**
Swagger has several native types referenced below:
*[integer|long|float|double|string|byte|binary|boolean|date|dateTime|password]* -
[see the swagger documentation](https://github.com/swagger-api/swagger-spec/blob/master/versions/2.0.md#data-types)

It consists of several sections which in term have sub-parameters:

**Supported Sections:**

## RESTQUERYPARAMETERS

Parameters to be appended to the URL in form of ?foo=bar
add *RESTQUERYPARAM*s below

## RESTURLPARAMETERS

Section of parameters placed in the URL in curly brackets, add *RESTURLPARAM*s below.

## RESTDESCRIPTION

Long text describing this route.

## RESTRETURNCODES

should consist of several *RESTRETURNCODE* tokens.

**Supported Tokens:**

## RESTREPLYBODY

Similar  to RESTBODYPARAM - just what the server will reply with.

## RESTHEADER

Up to 3 parameters.
* *[GET|POST|PUT|DELETE] <url>* url should start with a */*, it may contain parameters in curly brackets, that
  have to be documented in subsequent *RESTURLPARAM* tokens in the *RESTURLPARAMETERS* section.
* long description
* operationId - this is a uniq string that identifies the source parameter for this rest route. It defaults to a de-spaced `long description` - if set once it shouldn't be changed anymore.

## RESTURLPARAM

Consists of 3 values separated by ',':
Attributes:
  - *name*: name of the parameter
  - *type*:
  - *[required|optionas]* Optional is not supported anymore. Split the docublock into one with and one without.

Folowed by a long description.

## RESTALLBODYPARAM

This API has a schemaless json body (in doubt just plain ascii)

## RESTBODYPARAM

Attributes:
  - name - the name of the parameter
  - type - the swaggertype of the parameter
  - required/optional - whether the user can omit this parameter
  - subtype / format (can be empty)
    - subtype: if type is object or array, this references the enclosed variables.
               can be either a swaggertype, or a *RESTSTRUCT*
    - format: if type is a native swagger type, some support a format to specify them

## RESTSTRUCT

Groups a set of attributes under the `structure name` to an object that can be referenced
in other *RESTSTRUCT*s or *RESTBODYPARAM* attributes of type array or object

Attributes:
  - name - the name of the parameter
  - structure name - the **type** under which this structure can be reached (should be uniq!)
  - type - the swaggertype of the parameter (or another *RESTSTRUCT*...)
  - required/optional - whether the user can omit this parameter
  - subtype / format (can be empty)
    - subtype: if type is object or array, this references the enclosed variables.
               can be either a swaggertype, or a *RESTRUCT*
    - format: if type is a native swagger type, some support a format to specify them
