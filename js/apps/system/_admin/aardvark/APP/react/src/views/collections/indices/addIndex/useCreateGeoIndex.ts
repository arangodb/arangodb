import * as Yup from "yup";
import { useCreateIndex } from "./useCreateIndex";
import { commonFieldsMap } from "./IndexFieldsHelper";

const initialValues = {
  type: "geo",
  fields: commonFieldsMap.fields.initialValue,
  inBackground: commonFieldsMap.inBackground.initialValue,
  name: commonFieldsMap.name.initialValue,
  geoJson: false
};

const fields = [
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

const schema = Yup.object({
  fields: Yup.string().required("Fields are required")
});

type ValuesType = Omit<typeof initialValues, "fields"> & {
  fields: string[];
};

export const useCreateGeoIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof initialValues }) => {
    return onCreateIndex({
      ...values,
      fields: values.fields.split(",")
    });
  };
  return { onCreate, initialValues, schema, fields };
};
