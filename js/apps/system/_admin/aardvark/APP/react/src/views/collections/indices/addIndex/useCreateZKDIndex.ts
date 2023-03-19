import * as Yup from "yup";
import { commonFieldsMap, commonSchema } from "./IndexFieldsHelper";
import { useCreateIndex } from "./useCreateIndex";

const initialValues = {
  type: "zkd",
  fields: commonFieldsMap.fields.initialValue,
  inBackground: commonFieldsMap.inBackground.initialValue,
  name: commonFieldsMap.name.initialValue,
  fieldValueTypes: "double"
};

const fields = [
  commonFieldsMap.fields,
  commonFieldsMap.name,
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

const schema = Yup.object({
  ...commonSchema
});


type ValuesType = Omit<typeof initialValues, "fields"> & {
  fields: string[];
};

export const useCreateZKDIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof initialValues }) => {
    return onCreateIndex({
      ...values,
      fields: values.fields.split(",")
    });
  };
  return { onCreate, initialValues, schema, fields };
};
