import * as Yup from "yup";
import { commonFieldsMap, commonSchema } from "./IndexFieldsHelper";
import { useCreateIndex } from "./useCreateIndex";
import {
  INITIAL_VALUES as MDI_INITIAL_VALUES,
  mdiFieldsMap
} from "./useCreateMDIIndex";
export const INITIAL_VALUES = {
  ...MDI_INITIAL_VALUES,
  prefixFields: "",
  type: "mdi-prefixed"
};

export const FIELDS = [
  { ...commonFieldsMap.fields, isRequired: false },
  {
    label: "Prefix Fields",
    name: "prefixFields",
    type: "string",
    tooltip: "A comma-separated list of attribute names used as search prefix."
  },
  commonFieldsMap.name,
  mdiFieldsMap.storedValues,
  mdiFieldsMap.fieldValueTypes,
  commonFieldsMap.inBackground
];

export const SCHEMA = Yup.object().shape(
  {
    ...commonSchema,
    prefixFields: Yup.string().when("fields", {
      is: (fields: string) => !fields || fields.length === 0,
      then: schema =>
        schema.required("Either fields or prefix fields are required"),
      otherwise: schema => schema
    }),
    fields: Yup.string().when("prefixFields", {
      is: (prefixFields: string) => !prefixFields || prefixFields.length === 0,
      then: schema =>
        schema.required("Either fields or prefix fields are required"),
      otherwise: schema => schema
    })
  },
  [["fields", "prefixFields"]]
);

type ValuesType = Omit<
  typeof INITIAL_VALUES,
  "fields" | "storedValues" | "prefixFields"
> & {
  fields: string[];
  storedValues?: string[];
  prefixFields: string[];
};

export const useCreatePrefixedMDIIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof INITIAL_VALUES }) => {
    return onCreateIndex({
      ...values,
      storedValues: values.storedValues
        ? values.storedValues.split(",").map(field => field.trim())
        : undefined,
      fields: values.fields ? values.fields.split(",").map(field => field.trim()) : [],
      prefixFields: values.prefixFields.split(",").map(field => field.trim())
    });
  };
  return { onCreate };
};
