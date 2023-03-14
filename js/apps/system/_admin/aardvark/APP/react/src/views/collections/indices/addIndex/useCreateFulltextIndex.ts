import { toNumber } from "lodash";
import { useCreateIndex } from "./useCreateIndex";
import * as Yup from "yup";

const initialValues = {
  type: "fulltext",
  fields: "",
  minLength: 0,
  inBackground: true,
  name: ""
};

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
  return { onCreate, initialValues, schema };
};
