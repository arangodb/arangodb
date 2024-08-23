import { Box } from "@chakra-ui/react";
import Ajv from "ajv";
import { useFormikContext } from "formik";
import { ValidationError } from "jsoneditor-react";
import React, { useState } from "react";
import { ControlledJSONEditor } from "../../../../../components/jsonEditor/ControlledJSONEditor";
import { JSONErrors } from "../../../../../components/jsonEditor/JSONErrors";
import { useInvertedIndexJSONSchema } from "./useInvertedIndexJSONSchema";

const ajv = new Ajv({
  allErrors: true,
  verbose: true,
  discriminator: true,
  $data: true
});

export const InvertedIndexFormJSONEditor = ({
  isFormDisabled
}: {
  isFormDisabled?: boolean;
}) => {
  const { values, setValues } = useFormikContext();
  const { schema } = useInvertedIndexJSONSchema();
  const [errors, setErrors] = useState<ValidationError[]>();
  return (
    <Box height="100%" backgroundColor="white" position="relative" minWidth={0}>
      <ControlledJSONEditor
        value={values}
        isDisabled={isFormDisabled}
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
            if (JSON.stringify(json) !== JSON.stringify(values)) {
              setValues(json);
            }
          } catch (e) {
            // ignore
          }
        }}
        htmlElementProps={{
          style: {
            height: "calc(100% - 80px)",
            width: "100%"
          }
        }}
      />
      {!isFormDisabled && <JSONErrors errors={errors} />}
    </Box>
  );
};
