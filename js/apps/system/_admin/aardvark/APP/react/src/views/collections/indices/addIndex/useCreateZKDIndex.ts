import * as Yup from "yup";
import { commonFields, useCreateIndex } from "./useCreateIndex";

const initialValues = {
  type: "zkd",
  fields: "",
  fieldValueTypes: "double",
  inBackground: true,
  name: ""
};

const fields = [
  commonFields.fields,
  commonFields.name,
  {
    label: "Field Value Types",
    name: "fieldValueTypes",
    type: "text",
    isDisabled: true,
    tooltip:
      "The value type of the fields being indexed (only double supported for now)."
  },
  commonFields.inBackground
];

const schema = Yup.object({
  fields: Yup.string().required("Fields are required")
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
