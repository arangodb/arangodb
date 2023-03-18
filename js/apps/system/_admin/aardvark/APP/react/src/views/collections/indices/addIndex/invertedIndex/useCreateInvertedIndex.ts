import * as Yup from "yup";
import { commonFieldsMap } from "../IndexFieldsHelper";
import { useCreateIndex } from "../useCreateIndex";

/**
 * Index definition could be split on several blocks.
 * 
 * 1. Commit/Consolidation properties - same as for ArangoSearch Views:  could be found here https://www.arangodb.com/docs/stable/arangosearch-views.html. Starting from “cleanupIntervalStep“ and below. Mutable!  For this I suggest to have something similar to what we already have for ArangoSearch Views.

 * 2. “root“ fields properties: 
```
  analyzer: <analyzerName:string>,
  features: <analyzerFeatures: array, possible values [ "frequency", "position", "offset", "norm"], optional, default []>,
  includeAllFields: <bool, optional, default: false>,
  trackListPositions: <bool, optional, default: false>,
  searchField: <bool, optional, deafult: false>
```
 * 3. Fields definition.  In CE this is just an array of objects. But for EE we have “nested” fields with arbitrary nesting depth.  That is an array of fields objects.
    ```
        {
          name: <name:string>,
          analyzer: <analyzerName:string, optional>
          searchField: <bool, optional>
          features: <analyzerFeatures: array, possible values [ "frequency", "position", "offset", "norm"], optional, default []>   
          includeAllFields: <bool, optional>
          trackListPositions: <bool, optional>          
          nested: [
            // see below, EE only, optional
            ],
          },
        }
```
    And nested fields definitons looks like - same nested structure as we have had for ArangoSearch views fields in general.
```
  nested: 
        [ { "name":"subfoo",
              analyzer: <analyzerName:string, optional>,
              searchField: <bool, optional>,
              features: <analyzerFeatures: array, possible values [ "frequency", "position", "offset", "norm"], optional, default []>
              nested: [
                //sub-nested fields (optional)
              ]
             },
            .....
          ]
```
 * 4.Primary sort. Very like ArangoSearch views primary sort but defined a bit different - compression is moved inside primarySort object. Potentially there would be more fields.  
```
  primarySort: { fields: [
    // fields array as for arangosearch views
  ], compression: lz4}
```
*/

type AnalyzerFeatures = "frequency" | "position" | "offset" | "norm";

export type InvertedIndexFieldType = {
  name: string;
  analyzer?: string;
  features?: AnalyzerFeatures[];
  searchField?: boolean;
  includeAllFields?: boolean;
  trackListPositions?: boolean;
  nested?: Omit<
    InvertedIndexFieldType,
    "includeAllFields" | "trackListPositions"
  >[];
};

export type InvertedIndexValuesType = {
  type: string;
  name?: string;
  inBackground?: boolean;
  analyzer?: string;
  features?: AnalyzerFeatures[];
  includeAllFields?: boolean;
  trackListPositions?: boolean;
  searchField?: boolean;
  fields?: InvertedIndexFieldType[];
};

const initialValues = {
  type: "inverted",
  name: commonFieldsMap.fields.initialValue,
  inBackground: commonFieldsMap.inBackground.initialValue,
  analyzer: "",
  features: [],
  includeAllFields: false,
  trackListPositions: false,
  searchField: false,
  fields: []
};

const analyzerFeaturesOptions = [
  "frequency",
  "position",
  "offset",
  "norm"
].map(value => {
  return { value, label: value };
});

export const invertedIndexFieldsMap = {
  fields: {
    label: "Fields",
    name: "fields",
    isRequired: true,
    type: "custom",
    tooltip: "A comma-separated list of attribute paths."
  },
  analyzer: {
    label: "Analyzer",
    name: "analyzer",
    type: "custom"
  },
  features: {
    label: "Analyzer Features",
    name: "features",
    type: "multiSelect",
    options: analyzerFeaturesOptions
  },
  includeAllFields: {
    label: "Include All Fields",
    name: "includeAllFields",
    type: "boolean",
    initialValue: false
  },
  trackListPositions: {
    label: "Track List Positions",
    name: "trackListPositions",
    type: "boolean",
    initialValue: false
  },
  searchField: {
    label: "Search Field",
    name: "searchField",
    type: "boolean",
    initialValue: false
  }
};
const invertedIndexFields = [
  invertedIndexFieldsMap.fields,
  invertedIndexFieldsMap.analyzer,
  invertedIndexFieldsMap.features,
  invertedIndexFieldsMap.includeAllFields,
  invertedIndexFieldsMap.trackListPositions,
  invertedIndexFieldsMap.searchField,
  commonFieldsMap.name,
  commonFieldsMap.inBackground
];

const fieldSchema: any = Yup.object().shape({
  name: Yup.string().required(),
  inBackground: Yup.boolean(),
  analyzer: Yup.string(),
  features: Yup.array().of(Yup.string().required()),
  includeAllFields: Yup.boolean(),
  trackListPositions: Yup.boolean(),
  searchField: Yup.boolean(),
  nested: Yup.array().of(Yup.lazy(() => fieldSchema.default(undefined)))
});
const schema = Yup.object({
  fields: Yup.array()
    .of(Yup.array())
    .of(fieldSchema)
    .min(1, "At least one field is required")
    .required("At least one field is required")
});

export const useCreateInvertedIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<InvertedIndexValuesType>();
  const onCreate = async ({ values }: { values: InvertedIndexValuesType }) => {
    return onCreateIndex({
      ...values,
      fields:
        values.fields && values.fields.length > 0 ? values.fields : undefined,
      analyzer: values.analyzer || undefined,
      name: values.name || undefined
    });
  };
  return { onCreate, initialValues, schema, fields: invertedIndexFields };
};
