import Ajv from "ajv";
import ajvErrors from "ajv-errors";
import { useFormikContext } from "formik";
import React, { useState } from "react";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { JSONErrors } from "../../../components/jsonEditor/JSONErrors";
import { AnalyzerJSONFormSchema } from "../AnalyzerJSONSchemaHelper";

const ajv = new Ajv({
  allErrors: true,
  verbose: false,
  discriminator: true,
  $data: true
});
ajvErrors(ajv);
export const AnalyzerJSONForm = () => {
  const { values } = useFormikContext();
  const [errors, setErrors] = useState<any[]>([]);
  return (
    <>
      <ControlledJSONEditor
        value={values}
        mode={"code"}
        ajv={ajv}
        onValidationError={errors => {
          setErrors(errors);
        }}
        schema={AnalyzerJSONFormSchema}
        htmlElementProps={{
          style: {
            height: "calc(100vh - 300px)",
            width: "100%"
          }
        }}
      />
      <JSONErrors errors={errors} />
    </>
  );
};
