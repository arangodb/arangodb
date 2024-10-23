import * as Yup from "yup";
import { commonFieldsMap, commonSchema } from "../IndexFieldsHelper";
import { useCreateIndex } from "../useCreateIndex";

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
  cache?: boolean;
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
    cache?: boolean;
  };
  storedValues?: {
    fields: string[];
    compression: "lz4" | "none";
    cache?: boolean;
  }[];
  cleanupIntervalStep?: number;
  commitIntervalMsec?: number;
  consolidationIntervalMsec?: number;
  writebufferIdle?: number;
  writebufferActive?: number;
  writebufferSizeMax?: number;
  consolidationPolicy?: ConsolidationPolicy;
  fields?: InvertedIndexFieldType[];
  primaryKeyCache?: boolean;
  cache?: boolean;
};

const initialValues: InvertedIndexValuesType = {
  type: "inverted",
  name: commonFieldsMap.fields.initialValue,
  inBackground: commonFieldsMap.inBackground.initialValue,
  analyzer: "",
  features: [],
  cache: false,
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
  cache: {
    isDisabled: !window.frontendConfig.isEnterprise,
    label: "Cache",
    name: "cache",
    type: "boolean",
    group: "fields", 
    tooltip: window.frontendConfig.isEnterprise
      ? "Always cache field normalization values in memory for all fields by default."
      : "Field normalization value caching is available in Enterprise plans.",
  },
  fields: {
    label: "Fields",
    name: "fields",
    isRequired: true,
    type: "custom",
    tooltip:
      "Add field names that you want to be indexed here. Click on a field name to set field details.",
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
    group: "fields",
    tooltip: "Process all document attributes."
  },
  trackListPositions: {
    label: "Track List Positions",
    name: "trackListPositions",
    type: "boolean",
    initialValue: false,
    group: "fields",
    tooltip: "For array values track the value position in arrays."
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
  primaryKeyCache: {
    isDisabled: !window.frontendConfig.isEnterprise,
    label: "Primary Key Cache",
    name: "primaryKeyCache",
    type: "boolean",
    group: "general",
    tooltip: window.frontendConfig.isEnterprise
      ? "Always cache primary key columns in memory."
      : "Primary key column caching is available in Enterprise plans.",
  },
  storedValues: {
    label: "Stored Values",
    name: "storedValues",
    type: "custom"
  },

  writebufferIdle: {
    label: "Writebuffer Idle",
    name: "writebufferIdle",
    type: "number",
    group: "general",
    tooltip:
      "Maximum number of writers (segments) cached in the pool (default: 64, use 0 to disable, immutable)."
  },
  writebufferActive: {
    label: "Writebuffer Active",
    name: "writebufferActive",
    type: "number",
    group: "general",
    tooltip:
      "Maximum number of concurrent active writers (segments) that perform a transaction. Other writers (segments) wait till current active writers (segments) finish (default: 0, use 0 to disable, immutable)."
  },
  writebufferSizeMax: {
    label: "Writebuffer Size Max",
    name: "writebufferSizeMax",
    type: "number",
    group: "general",
    tooltip:
      "Maximum memory byte size per writer (segment) before a writer (segment) flush is triggered. 0 value turns off this limit for any writer (buffer) and data will be flushed periodically based on the value defined for the flush thread (ArangoDB server startup option). 0 value should be used carefully due to high potential memory consumption (default: 33554432, use 0 to disable, immutable)."
  },
  cleanupIntervalStep: {
    label: "Cleanup Interval Step",
    name: "cleanupIntervalStep",
    type: "number",
    group: "general",
    tooltip:
      "Index waits at least this many commits between removing unused files in its data directory."
  },
  commitIntervalMsec: {
    label: "Commit Interval (msec)",
    name: "commitIntervalMsec",
    type: "number",
    group: "general",
    tooltip:
      "Wait at least this many milliseconds between committing data store changes and making documents visible to queries."
  },
  consolidationIntervalMsec: {
    label: "Consolidation Interval (msec)",
    name: "consolidationIntervalMsec",
    type: "number",
    group: "general",
    tooltip:
      "Wait at least this many milliseconds between index segments consolidations."
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
  invertedIndexFieldsMap.cache,
  invertedIndexFieldsMap.features,
  invertedIndexFieldsMap.includeAllFields,
  invertedIndexFieldsMap.trackListPositions,
  invertedIndexFieldsMap.searchField,
  invertedIndexFieldsMap.primarySort,
  invertedIndexFieldsMap.storedValues,
  invertedIndexFieldsMap.primaryKeyCache,
  invertedIndexFieldsMap.writebufferIdle,
  invertedIndexFieldsMap.writebufferActive,
  invertedIndexFieldsMap.writebufferSizeMax,
  invertedIndexFieldsMap.cleanupIntervalStep,
  invertedIndexFieldsMap.commitIntervalMsec,
  invertedIndexFieldsMap.consolidationIntervalMsec,
  invertedIndexFieldsMap.consolidationPolicy,
  { ...commonFieldsMap.inBackground, group: "general" }
];

const fieldSchema: any = Yup.object().shape({
  name: Yup.string().required(),
  cache: Yup.boolean(),
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
  includeAllFields: Yup.boolean(),
  fields: Yup.array()
    .of(fieldSchema)
    .when("includeAllFields", {
      is: false,
      then: () =>
        Yup.array()
          .of(fieldSchema)
          .min(1, "At least one field is required")
          .required("At least one field is required"),
      otherwise: () => Yup.array().of(fieldSchema)
    })
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
        fields: primarySortFields,
        cache: values.primarySort?.cache
      },
      storedValues: storedValues
    });
  };
  const { initialValues, schema, fields } = useInvertedIndexFieldsData();
  return { onCreate, initialValues, schema, fields };
};
