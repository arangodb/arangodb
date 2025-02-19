import { Box } from "@chakra-ui/react";
import Ajv from "ajv";
import { useFormikContext } from "formik";
import React from "react";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { JSONErrors } from "../../../components/jsonEditor/JSONErrors";
import { useEditViewContext } from "../editView/EditViewContext";
import { useArangoSearchJSONSchema } from "./SearchJSONSchema";

const ajv = new Ajv({
  allErrors: true,
  verbose: true,
  discriminator: true,
  $data: true
});

export const ArangoSearchJSONEditor = () => {
  const { values, setValues } = useFormikContext();
  const { initialView, setErrors, errors } = useEditViewContext();
  const { schema } = useArangoSearchJSONSchema({ view: initialView });
  return (
    <Box height="100%" backgroundColor="white" position="relative" minWidth={0}>
      <ControlledJSONEditor
        value={values}
        onValidationError={errors => {
          setErrors(errors);
        }}
        mode={"code"}
        ajv={ajv}
        history
        schema={schema}
        onChange={json => {
          if (JSON.stringify(json) !== JSON.stringify(values)) {
            setValues(json);
          }
        }}
        htmlElementProps={{
          style: {
            height: "100%",
            width: "100%"
          }
        }}
      />
      <JSONErrors errors={errors} />
    </Box>
  );
};
