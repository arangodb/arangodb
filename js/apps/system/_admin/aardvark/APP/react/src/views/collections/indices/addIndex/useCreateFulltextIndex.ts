import { toNumber } from "lodash";
import { useCreateIndex } from "./useCreateIndex";
import { commonFieldsMap, commonSchema } from "./IndexFieldsHelper";
import * as Yup from "yup";

const initialValues = {
  type: "fulltext",
  fields: commonFieldsMap.fields.initialValue,
  inBackground: commonFieldsMap.inBackground.initialValue,
  name: commonFieldsMap.name.initialValue,
  minLength: 0
};

const fields = [
  commonFieldsMap.fields,
  commonFieldsMap.name,
  {
    label: "Min. Length",
    name: "minLength",
    type: "number",
    tooltip:
      "Minimum character length of words to index. Will default to a server-defined value if unspecified. It is thus recommended to set this value explicitly when creating the index."
  },
  commonFieldsMap.inBackground
];

const schema = Yup.object({
  ...commonSchema
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
