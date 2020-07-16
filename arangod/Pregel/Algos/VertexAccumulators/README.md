# Vertex Accumulators


## What are Vertex Accumulators?

TODO: Fill me 

## Example Config of a Vertex Accumulator Algorithm

{ "resultField": "SCC",
  "accumulators": [
    { "type": "min",
      "valueType": "int",
      "storeSender": true,
      "initExpression": [ "if" ],
      "updateExpression": [ "update" ] },
  ] }
