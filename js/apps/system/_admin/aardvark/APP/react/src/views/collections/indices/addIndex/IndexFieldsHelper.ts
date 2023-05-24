import * as Yup from "yup";
import { getNormalizedByteLengthTest } from "../../../../utils/yupHelper";

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
const traditionalNameSchema = Yup.string()
  .max(256, "Index name max length is 256.")
  .matches(/^[a-zA-Z]/, "Index name must always start with a letter.")
  .matches(/^[a-zA-Z0-9\-_]*$/, 'Only symbols, "_" and "-" are allowed.');

const extendedNameSchema = Yup.string()
  .test(
    "normalizedByteLength",
    "Index name max length is 256 bytes.",
    getNormalizedByteLengthTest(256, "Index name max length is 256 bytes.")
  )
  .matches(/^(?![0-9])/, "Index name cannot start with a number.")
  .matches(/^\S(.*\S)?$/, "Index name cannot contain leading/trailing spaces.")
  .matches(/^(?!.*[/])/, "Index name cannot contain a forward slash (/).")
  .matches(
    window.arangoValidationHelper.getControlCharactersRegex(),
    "Index name cannot contain control characters (0-31)."
  );
const extendedNames = window.frontendConfig.extendedNames;

export const commonSchema = {
  fields: Yup.string().required("Fields are required"),
  inBackground: Yup.boolean(),
  name: extendedNames ? extendedNameSchema : traditionalNameSchema
};
