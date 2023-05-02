import * as Yup from "yup";
import { commonFieldsMap, commonSchema } from "./IndexFieldsHelper";
import { useCreateIndex } from "./useCreateIndex";

export const FIELDS = [
  commonFieldsMap.fields,
  commonFieldsMap.name,
  {
    label: "Extra stored values",
    name: "storedValues",
    type: "string",
    tooltip:
      "A comma-separated list of extra attribute paths. The values of these attributes will be stored in the index as well. They can be used for projections, but cannot be used for filtering or sorting."
  },
  {
    label: "Unique",
    name: "unique",
    type: "boolean",
    tooltip: "If true, then create a unique index."
  },
  {
    label: "Sparse",
    name: "sparse",
    type: "boolean",
    tooltip:
      "If true, then create a sparse index. Sparse indexes do not include documents for which the index attributes do not exist or have a value of null. Sparse indexes will exclude any such documents, so they can save space, but are less versatile than non-sparse indexes. For example, sparse indexes may not be usable for sort operations, range or point lookup queries if the query optimizer cannot prove that the index query range excludes null values."
  },
  {
    label: "Deduplicate array values",
    name: "deduplicate",
    type: "boolean",
    tooltip:
      "Duplicate index values from the same document into a unique array index will lead to an error or not."
  },
  {
    label: "Maintain index selectivity estimates",
    name: "estimates",
    type: "boolean",
    tooltip:
      "Maintain index selectivity estimates for this index. This option can be turned off for non-unique-indexes to improve efficiency of write operations, but will lead to the optimizer being unaware of the data distribution inside this index."
  },
  {
    label: "Enable in-memory cache for index lookups",
    name: "cacheEnabled",
    type: "boolean",
    tooltip:
      "Cache index lookup values in memory for faster lookups of the same values. Note that the cache will be initially empty and be populated lazily during lookups. The cache can only be used for equality lookups on all index attributes. It cannot be used for range scans, partial lookups or sorting."
  },
  commonFieldsMap.inBackground
];

export const INITIAL_VALUES = {
  type: "persistent",
  fields: commonFieldsMap.fields.initialValue,
  name: commonFieldsMap.fields.initialValue,
  storedValues: "",
  unique: false,
  sparse: false,
  deduplicate: false,
  estimates: true,
  cacheEnabled: false,
  inBackground: commonFieldsMap.inBackground.initialValue
};

export const SCHEMA = Yup.object({
  ...commonSchema
});

type ValuesType = Omit<typeof INITIAL_VALUES, "fields" | "storedValues"> & {
  fields: string[];
  storedValues?: string[];
};

export const useCreatePersistentIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof INITIAL_VALUES }) => {
    return onCreateIndex({
      ...values,
      storedValues: values.storedValues ? values.storedValues.split(",") : undefined,
      fields: values.fields.split(",")
    });
  };
  return { onCreate };
};
