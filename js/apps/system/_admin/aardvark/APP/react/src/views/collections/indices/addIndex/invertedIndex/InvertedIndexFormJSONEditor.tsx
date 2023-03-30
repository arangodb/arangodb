import { Box } from "@chakra-ui/react";
import Ajv from "ajv";
import { useFormikContext } from "formik";
import { ValidationError } from "jsoneditor-react";
import React, { useState } from "react";
import { ControlledJSONEditor } from "./ControlledJSONEditor";
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
            height: "calc(100% - 80px)",
            width: "100%"
          }
        }}
      />
      {!isFormDisabled && <JSONErrors errors={errors} />}
    </Box>
  );
};

const JSONErrors = ({ errors }: { errors?: ValidationError[] }) => {
  if (!errors || errors.length === 0) {
    return null;
  }
  return (
    <Box
      maxHeight={"80px"}
      paddingX="2"
      paddingY="1"
      fontSize="sm"
      color="red"
      background="red.100"
      overflow={"auto"}
    >
      {errors.map(error => {
        return (
          <Box>{`${error.keyword} error: ${
            error.message
          }. Schema: ${JSON.stringify(error.params)}`}</Box>
        );
      })}
    </Box>
  );
};
