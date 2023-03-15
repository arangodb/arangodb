import Ajv from "ajv";
import { useFormikContext } from "formik";
import React from "react";
import { ControlledJSONEditor } from "./ControlledJSONEditor";
import { useInvertedIndexFormSchema } from "./useInvertedIndexFormSchema";

const ajv = new Ajv({
  allErrors: true,
  verbose: true,
  discriminator: true,
  $data: true
});

export const InvertedIndexFormJSONEditor = () => {
  const { values, setValues } = useFormikContext();
  const schema = useInvertedIndexFormSchema();
  return (
    <ControlledJSONEditor
      value={values}
      mode={"code"}
      ajv={ajv}
      history
      schema={schema}
      onChange={json => {
        console.log("on change!", { json }, setValues);
        // setValues(json);
      }}
      htmlElementProps={{
        style: {
          height: "calc(100% - 40px)",
          width: "100%"
        }
      }}
    />
  );
};
