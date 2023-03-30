import * as Yup from "yup";

export const commonFieldsMap = {
  fields: {
    label: "Fields",
    name: "fields",
    type: "text",
    isRequired: true,
    tooltip: "A comma-separated list of attribute paths.",
    initialValue: ""
  },
  name: {
    label: "Name",
    name: "name",
    type: "text",
    tooltip:
      "Index name. If left blank, the name will be auto-generated. Example: byValue",
    initialValue: ""
  },
  inBackground: {
    label: "Create in background",
    name: "inBackground",
    type: "boolean",
    initialValue: true,
    tooltip: "Create the index in background."
  }
};

export const commonSchema = {
  fields: Yup.string().required("Fields are required"),
  inBackground: Yup.boolean()
};
