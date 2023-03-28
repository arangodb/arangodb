import { toNumber } from "lodash";
import * as Yup from "yup";
import { commonFieldsMap, commonSchema } from "./IndexFieldsHelper";
import { useCreateIndex } from "./useCreateIndex";

export const INITIAL_VALUES = {
  type: "ttl",
  fields: commonFieldsMap.fields.initialValue,
  inBackground: commonFieldsMap.inBackground.initialValue,
  name: commonFieldsMap.name.initialValue,
  expireAfter: 0
};

export const FIELDS = [
  commonFieldsMap.fields,
  commonFieldsMap.name,
  {
    label: "Documents expire after (s)",
    name: "expireAfter",
    type: "number",
    tooltip:
      "Number of seconds to be added to the timestamp attribute value of each document. If documents have reached their expiration timepoint, they will eventually get deleted by a background process."
  },
  commonFieldsMap.inBackground
];

export const SCHEMA = Yup.object({
  ...commonSchema
});

type ValuesType = Omit<typeof INITIAL_VALUES, "fields"> & {
  fields: string[];
};

export const useCreateTTLIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof INITIAL_VALUES }) => {
    return onCreateIndex({
      ...values,
      expireAfter: toNumber(values.expireAfter),
      fields: values.fields.split(",")
    });
  };
  return { onCreate };
};
