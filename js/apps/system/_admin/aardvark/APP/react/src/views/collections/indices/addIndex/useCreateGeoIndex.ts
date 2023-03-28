import * as Yup from "yup";
import { useCreateIndex } from "./useCreateIndex";
import { commonFieldsMap, commonSchema } from "./IndexFieldsHelper";

export const INITIAL_VALUES = {
  type: "geo",
  fields: commonFieldsMap.fields.initialValue,
  inBackground: commonFieldsMap.inBackground.initialValue,
  name: commonFieldsMap.name.initialValue,
  geoJson: false
};

export const FIELDS = [
  commonFieldsMap.fields,
  commonFieldsMap.name,
  {
    label: "Geo JSON",
    name: "geoJson",
    type: "boolean",
    tooltip:
      "Set geoJson to true if the coordinates stored in the specified attribute are arrays in the form [longitude,latitude]."
  },
  commonFieldsMap.inBackground
];

export const SCHEMA = Yup.object({
  ...commonSchema
});

type ValuesType = Omit<typeof INITIAL_VALUES, "fields"> & {
  fields: string[];
};

export const useCreateGeoIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof INITIAL_VALUES }) => {
    return onCreateIndex({
      ...values,
      fields: values.fields.split(",")
    });
  };
  return { onCreate };
};
