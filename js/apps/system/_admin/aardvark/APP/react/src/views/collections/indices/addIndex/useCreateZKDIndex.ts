import * as Yup from "yup";
import { commonFieldsMap, commonSchema } from "./IndexFieldsHelper";
import { useCreateIndex } from "./useCreateIndex";

export const INITIAL_VALUES = {
  type: "zkd",
  fields: commonFieldsMap.fields.initialValue,
  storedValues: "",
  inBackground: commonFieldsMap.inBackground.initialValue,
  name: commonFieldsMap.name.initialValue,
  fieldValueTypes: "double"
};

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
    label: "Field Value Types",
    name: "fieldValueTypes",
    type: "text",
    isDisabled: true,
    tooltip:
      "The value type of the fields being indexed (only double supported for now)."
  },
  commonFieldsMap.inBackground
];

export const SCHEMA = Yup.object({
  ...commonSchema
});

type ValuesType = Omit<typeof INITIAL_VALUES, "fields" | "storedValues"> & {
  fields: string[];
  storedValues?: string[];
};

export const useCreateZKDIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof INITIAL_VALUES }) => {
    return onCreateIndex({
      ...values,
      storedValues: values.storedValues ? values.storedValues.split(",").map(field => field.trim()) : undefined,
      fields: values.fields.split(",").map(field => field.trim())
    });
  };
  return { onCreate };
};
