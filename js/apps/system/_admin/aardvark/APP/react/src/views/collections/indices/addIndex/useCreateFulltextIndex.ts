import { toNumber } from "lodash";
import { useCreateIndex } from "./useCreateIndex";
import { commonFieldsMap, commonSchema } from "./IndexFieldsHelper";
import * as Yup from "yup";

export const INITIAL_VALUES = {
  type: "fulltext",
  fields: commonFieldsMap.fields.initialValue,
  inBackground: commonFieldsMap.inBackground.initialValue,
  name: commonFieldsMap.name.initialValue,
  minLength: 0
};

export const FIELDS = [
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

export const SCHEMA = Yup.object({
  ...commonSchema
});

type ValuesType = Omit<typeof INITIAL_VALUES, "fields"> & {
  fields: string[];
};

export const useCreateFulltextIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof INITIAL_VALUES }) => {
    return onCreateIndex({
      ...values,
      minLength: toNumber(values.minLength),
      fields: values.fields.split(",")
    });
  };
  return { onCreate };
};
