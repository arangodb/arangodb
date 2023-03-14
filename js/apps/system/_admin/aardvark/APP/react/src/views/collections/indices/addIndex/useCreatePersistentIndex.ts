import {
  commonFields,
  commonPersistentIndexFields,
  useCreateIndex
} from "./useCreateIndex";
import * as Yup from "yup";

const initialValues = {
  type: "persistent",
  fields: commonFields.fields.initialValue,
  name: commonFields.fields.initialValue,
  unique: commonPersistentIndexFields.unique.initialValue,
  sparse: commonPersistentIndexFields.sparse.initialValue,
  deduplicate: commonPersistentIndexFields.deduplicate.initialValue,
  estimates: commonPersistentIndexFields.estimates.initialValue,
  cacheEnabled: commonPersistentIndexFields.estimates.initialValue,
  inBackground: commonFields.inBackground.initialValue
};

const fields = [
  commonFields.fields,
  commonFields.name,
  commonPersistentIndexFields.unique,
  commonPersistentIndexFields.sparse,
  commonPersistentIndexFields.deduplicate,
  commonPersistentIndexFields.estimates,
  commonPersistentIndexFields.cacheEnabled,
  commonFields.inBackground
];

const schema = Yup.object({
  fields: Yup.string().required("Fields are required")
});

type ValuesType = Omit<typeof initialValues, "fields"> & { fields: string[] };

export const useCreatePersistentIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof initialValues }) => {
    return onCreateIndex({
      ...values,
      fields: values.fields.split(",")
    });
  };
  return { onCreate, initialValues, schema, fields };
};
