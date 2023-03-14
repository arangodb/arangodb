import { useCreateIndex } from "./useCreateIndex";
import {
  persistentIndexFields,
  persistentIndexInitialValues
} from "./IndexFieldsHelper";
import * as Yup from "yup";

const initialValues = {
  type: "hash",
  ...persistentIndexInitialValues
};

const schema = Yup.object({
  fields: Yup.string().required("Fields are required")
});

type ValuesType = Omit<typeof initialValues, "fields"> & { fields: string[] };

export const useCreateHashIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof initialValues }) => {
    return onCreateIndex({
      ...values,
      fields: values.fields.split(",")
    });
  };
  return { onCreate, initialValues, schema, fields: persistentIndexFields };
};
