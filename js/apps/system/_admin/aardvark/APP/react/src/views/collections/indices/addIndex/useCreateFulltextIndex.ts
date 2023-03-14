import { toNumber } from "lodash";
import { commonFields, useCreateIndex } from "./useCreateIndex";
import * as Yup from "yup";

const initialValues = {
  type: "fulltext",
  fields: "",
  minLength: 0,
  inBackground: true,
  name: ""
};

const fields = [
  commonFields.fields,
  commonFields.name,
  {
    label: "Min. Length",
    name: "minLength",
    type: "number",
    tooltip:
      "Minimum character length of words to index. Will default to a server-defined value if unspecified. It is thus recommended to set this value explicitly when creating the index."
  },
  commonFields.inBackground
];

const schema = Yup.object({
  fields: Yup.string().required("Fields are required")
});

type ValuesType = Omit<typeof initialValues, "fields"> & {
  fields: string[];
};

export const useCreateFulltextIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof initialValues }) => {
    return onCreateIndex({
      ...values,
      minLength: toNumber(values.minLength),
      fields: values.fields.split(",")
    });
  };
  return { onCreate, initialValues, schema, fields };
};
