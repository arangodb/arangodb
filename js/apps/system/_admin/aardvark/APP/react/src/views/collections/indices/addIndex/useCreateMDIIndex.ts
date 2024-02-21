import * as Yup from "yup";
import { commonFieldsMap, commonSchema } from "./IndexFieldsHelper";
import { useCreateIndex } from "./useCreateIndex";

export const INITIAL_VALUES = {
  type: "mdi",
  fields: commonFieldsMap.fields.initialValue,
  storedValues: "",
  inBackground: commonFieldsMap.inBackground.initialValue,
  name: commonFieldsMap.name.initialValue,
  fieldValueTypes: "double",
  estimates: true,
  unique: false,
  sparse: false
};

export const mdiFieldsMap = {
  storedValues: {
    label: "Extra stored values",
    name: "storedValues",
    type: "string",
    tooltip:
      "A comma-separated list of extra attribute paths. The values of these attributes will be stored in the index as well. They can be used for projections, but cannot be used for filtering or sorting."
  },
  fieldValueTypes: {
    label: "Field Value Types",
    name: "fieldValueTypes",
    type: "text",
    isDisabled: true,
    tooltip:
      "The value type of the fields being indexed (only double supported for now)."
  },
  estimates: {
    label: "Maintain index selectivity estimates",
    name: "estimates",
    type: "boolean",
    tooltip:
      "Maintain index selectivity estimates for this index. This option can be turned off for non-unique-indexes to improve efficiency of write operations, but will lead to the optimizer being unaware of the data distribution inside this index."
  },
  unique: {
    label: "Unique",
    name: "unique",
    type: "boolean",
    tooltip: "If true, then create a unique index."
  },
  sparse: {
    label: "Sparse",
    name: "sparse",
    type: "boolean",
    tooltip:
      "If true, then create a sparse index. Sparse indexes do not include documents for which the index attributes do not exist or have a value of null. Sparse indexes will exclude any such documents, so they can save space, but are less versatile than non-sparse indexes. For example, sparse indexes may not be usable for sort operations, range or point lookup queries if the query optimizer cannot prove that the index query range excludes null values."
  }
};
export const FIELDS = [
  commonFieldsMap.fields,
  commonFieldsMap.name,
  mdiFieldsMap.storedValues,
  mdiFieldsMap.fieldValueTypes,
  mdiFieldsMap.estimates,
  mdiFieldsMap.unique,
  mdiFieldsMap.sparse,
  commonFieldsMap.inBackground
];

export const SCHEMA = Yup.object({
  ...commonSchema
});

type ValuesType = Omit<typeof INITIAL_VALUES, "fields" | "storedValues"> & {
  fields: string[];
  storedValues?: string[];
};

export const useCreateMDIIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof INITIAL_VALUES }) => {
    return onCreateIndex({
      ...values,
      storedValues: values.storedValues
        ? values.storedValues.split(",").map(field => field.trim())
        : undefined,
      fields: values.fields.split(",").map(field => field.trim())
    });
  };
  return { onCreate };
};
