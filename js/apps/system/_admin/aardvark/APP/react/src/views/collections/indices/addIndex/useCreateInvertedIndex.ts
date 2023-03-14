import * as Yup from "yup";
import { commonFieldsMap } from "./IndexFieldsHelper";
import { useCreateIndex } from "./useCreateIndex";

const initialValues = {
  type: "inverted",
  fields: commonFieldsMap.fields.initialValue,
  name: commonFieldsMap.fields.initialValue,
  inBackground: commonFieldsMap.inBackground.initialValue
};

const invertedIndexFields = [
  { ...commonFieldsMap.fields, isRequired: false },
  commonFieldsMap.name,
  commonFieldsMap.inBackground
];

const schema = Yup.object({});

type ValuesType = Omit<typeof initialValues, "fields"> & { fields: string[] };

export const useCreateInvertedIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof initialValues }) => {
    return onCreateIndex({
      ...values,
      fields: values.fields.split(",")
    });
  };
  return { onCreate, initialValues, schema, fields: invertedIndexFields };
};
