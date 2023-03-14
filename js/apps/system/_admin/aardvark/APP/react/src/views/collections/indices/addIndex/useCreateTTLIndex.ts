import { toNumber } from "lodash";
import { commonFields, useCreateIndex } from "./useCreateIndex";
import * as Yup from "yup";

const initialValues = {
  type: "ttl",
  fields: "",
  expireAfter: 0,
  inBackground: true,
  name: ""
};

const fields = [
  commonFields.fields,
  commonFields.name,
  {
    label: "Documents expire after (s)",
    name: "expireAfter",
    type: "number",
    tooltip:
      "Number of seconds to be added to the timestamp attribute value of each document. If documents have reached their expiration timepoint, they will eventually get deleted by a background process."
  },
  commonFields.inBackground
];

const schema = Yup.object({
  fields: Yup.string().required("Fields are required")
});

type ValuesType = Omit<typeof initialValues, "fields"> & {
  fields: string[];
};

export const useCreateTTLIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof initialValues }) => {
    return onCreateIndex({
      ...values,
      expireAfter: toNumber(values.expireAfter),
      fields: values.fields.split(",")
    });
  };
  return { onCreate, initialValues, schema, fields };
};
