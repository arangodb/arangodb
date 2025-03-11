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
  const { initialView, setErrors, errors, setIsFormDisabled } =
    useEditViewContext();
  const { schema } = useArangoSearchJSONSchema({ view: initialView });

  return (
    <Box height="100%" backgroundColor="white" position="relative" minWidth={0}>
      <JSONErrors errors={errors} />
      <ControlledJSONEditor
        value={values}
        onValidationError={errors => {
          setErrors(errors);
        }}
        mode={"code"}
        ajv={ajv}
        history
        schema={schema}
        onChangeText={jsonString => {
          try {
            const json = JSON.parse(jsonString);
            setIsFormDisabled(false);
            if (JSON.stringify(json) !== JSON.stringify(values)) {
              setValues(json);
            }
          } catch (e) {
            setIsFormDisabled(true);
          }
        }}
        onChange={json => {
          try {
            const jsonString = JSON.stringify(json);
            const jsonParsed = JSON.parse(jsonString);
            setIsFormDisabled(false);
            if (JSON.stringify(jsonParsed) !== JSON.stringify(values)) {
              setValues(json);
            }
          } catch (e) {
            setIsFormDisabled(true);
          }
        }}
        htmlElementProps={{
          style: {
            height: "100%",
            width: "100%"
          }
        }}
      />
    </Box>
  );
};
