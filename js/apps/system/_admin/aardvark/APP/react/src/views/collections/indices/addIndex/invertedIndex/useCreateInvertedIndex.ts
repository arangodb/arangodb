import * as Yup from "yup";
import { commonFieldsMap, commonSchema } from "../IndexFieldsHelper";
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
type BytesAccumConsolidationPolicy = {
  type: string;
  threshold?: number;
};

type TierConsolidationPolicy = {
  type: string;
  segmentsMin?: number;
  segmentsMax?: number;
  segmentsBytesMax?: number;
  segmentsBytesFloor?: number;
  minScore?: number;
};

type ConsolidationPolicy =
  | TierConsolidationPolicy
  | BytesAccumConsolidationPolicy;

type AnalyzerFeatures = "frequency" | "position" | "offset" | "norm";

type PrimarySortFieldType = {
  field: string;
  direction: "asc" | "desc";
};

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
  primarySort?: {
    fields: PrimarySortFieldType[];
    compression: "lz4" | "none";
  };
  storedValues?: {
    fields: string[];
    compression: "lz4" | "none";
  }[];
  cleanupIntervalStep?: number;
  commitIntervalMsec?: number;
  consolidationIntervalMsec?: number;
  writebufferIdle?: number;
  writebufferActive?: number;
  writebufferSizeMax?: number;
  consolidationPolicy?: ConsolidationPolicy;
  fields?: InvertedIndexFieldType[];
};

const initialValues: InvertedIndexValuesType = {
  type: "inverted",
  name: commonFieldsMap.fields.initialValue,
  inBackground: commonFieldsMap.inBackground.initialValue,
  analyzer: "",
  features: [],
  includeAllFields: false,
  trackListPositions: false,
  searchField: false,
  fields: [],
  cleanupIntervalStep: 2,
  commitIntervalMsec: 1000,
  consolidationIntervalMsec: 1000,
  writebufferIdle: 64,
  writebufferActive: 0,
  writebufferSizeMax: 33554432,
  primarySort: {
    fields: [{ field: "", direction: "asc" }],
    compression: "lz4"
  },
  storedValues: [
    {
      fields: [],
      compression: "lz4"
    }
  ],
  consolidationPolicy: {
    type: "tier",
    segmentsMin: 1,
    segmentsMax: 10,
    segmentsBytesMax: 5368709120,
    segmentsBytesFloor: 2097152,
    minScore: 0
  }
};

const analyzerFeaturesOptions = ["frequency", "position", "offset", "norm"].map(
  value => {
    return { value, label: value };
  }
);

export const invertedIndexFieldsMap = {
  fields: {
    label: "Fields",
    name: "fields",
    isRequired: true,
    type: "custom",
    tooltip: "A comma-separated list of attribute paths.",
    group: "fields"
  },
  analyzer: {
    label: "Analyzer",
    name: "analyzer",
    type: "custom",
    group: "fields"
  },
  features: {
    label: "Analyzer Features",
    name: "features",
    type: "multiSelect",
    options: analyzerFeaturesOptions,
    group: "fields"
  },
  includeAllFields: {
    label: "Include All Fields",
    name: "includeAllFields",
    type: "boolean",
    initialValue: false,
    group: "fields"
  },
  trackListPositions: {
    label: "Track List Positions",
    name: "trackListPositions",
    type: "boolean",
    initialValue: false,
    group: "fields"
  },
  searchField: {
    label: "Search Field",
    name: "searchField",
    type: "boolean",
    initialValue: false,
    group: "fields"
  },
  primarySort: {
    label: "Primary Sort",
    name: "primarySort",
    type: "custom"
  },
  storedValues: {
    label: "Stored Values",
    name: "storedValues",
    type: "custom"
  },
  cleanupIntervalStep: {
    label: "Cleanup Interval Step",
    name: "cleanupIntervalStep",
    type: "number",
    group: "general"
  },
  commitIntervalMsec: {
    label: "Commit Interval (msec)",
    name: "commitIntervalMsec",
    type: "number",
    group: "general"
  },
  consolidationIntervalMsec: {
    label: "Consolidation Interval (msec)",
    name: "consolidationIntervalMsec",
    type: "number",
    group: "general"
  },
  writebufferIdle: {
    label: "Writebuffer Idle",
    name: "writebufferIdle",
    type: "number",
    group: "general"
  },
  writebufferActive: {
    label: "Writebuffer Active",
    name: "writebufferActive",
    type: "number",
    group: "general"
  },
  writebufferSizeMax: {
    label: "Writebuffer Size Max",
    name: "writebufferSizeMax",
    type: "number",
    group: "general"
  },
  consolidationPolicy: {
    label: "Consolidation Policy",
    name: "consolidationPolicy",
    type: "custom"
  }
};

const invertedIndexFields = [
  invertedIndexFieldsMap.fields,
  { ...commonFieldsMap.name, group: "general" },
  invertedIndexFieldsMap.analyzer,
  invertedIndexFieldsMap.features,
  invertedIndexFieldsMap.includeAllFields,
  invertedIndexFieldsMap.trackListPositions,
  invertedIndexFieldsMap.searchField,
  invertedIndexFieldsMap.primarySort,
  invertedIndexFieldsMap.storedValues,
  invertedIndexFieldsMap.cleanupIntervalStep,
  invertedIndexFieldsMap.commitIntervalMsec,
  invertedIndexFieldsMap.consolidationIntervalMsec,
  invertedIndexFieldsMap.writebufferIdle,
  invertedIndexFieldsMap.writebufferActive,
  invertedIndexFieldsMap.writebufferSizeMax,
  invertedIndexFieldsMap.consolidationPolicy,
  { ...commonFieldsMap.inBackground, group: "general" }
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
  ...commonSchema,
  fields: Yup.array()
    .of(fieldSchema)
    .min(1, "At least one field is required")
    .required("At least one field is required")
});

export const useInvertedIndexFieldsData = () => {
  return { initialValues, schema, fields: invertedIndexFields };
};

export const useCreateInvertedIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<InvertedIndexValuesType>();
  const onCreate = async ({ values }: { values: InvertedIndexValuesType }) => {
    const primarySortFields =
      values.primarySort?.fields.filter(field => !!field.field) || [];
    const storedValues = values.storedValues?.filter(value => !!value) || [];
    return onCreateIndex({
      ...values,
      fields:
        values.fields && values.fields.length > 0 ? values.fields : undefined,
      analyzer: values.analyzer || undefined,
      name: values.name || undefined,
      primarySort: {
        compression: values.primarySort?.compression || "lz4",
        fields: primarySortFields
      },
      storedValues: storedValues
    });
  };
  const { initialValues, schema, fields } = useInvertedIndexFieldsData();
  return { onCreate, initialValues, schema, fields };
};
