import { useCreateIndex } from "./useCreateIndex";
import * as Yup from "yup";

const initialValues = {
  type: "skiplist",
  fields: "",
  name: "",
  unique: false,
  sparse: false,
  deduplicate: false,
  estimates: true,
  cacheEnabled: false,
  inBackground: true
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
  return { onCreate, initialValues, schema };
};
