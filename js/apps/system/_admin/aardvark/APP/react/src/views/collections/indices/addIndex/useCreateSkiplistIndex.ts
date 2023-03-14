import * as Yup from "yup";
import {
  persistentIndexFields,
  persistentIndexInitialValues
} from "./IndexFieldsHelper";
import { useCreateIndex } from "./useCreateIndex";

const initialValues = {
  type: "skiplist",
  ...persistentIndexInitialValues
};

const schema = Yup.object({
  fields: Yup.string().required("Fields are required")
});

type ValuesType = Omit<typeof initialValues, "fields"> & { fields: string[] };

export const useCreateSkiplistIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof initialValues }) => {
    return onCreateIndex({
      ...values,
      fields: values.fields.split(",")
    });
  };
  return { onCreate, initialValues, schema, fields: persistentIndexFields };
};
