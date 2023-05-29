import { Box } from "@chakra-ui/react";
import Ajv, { JSONSchemaType } from "ajv";
import { useFormikContext } from "formik";
import React, { useEffect } from "react";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { JSONErrors } from "../../../components/jsonEditor/JSONErrors";
import { useEditViewContext } from "../editView/EditViewContext";
import { useArangoSearchJSONSchema } from "../SearchJSONSchema";
import { ArangoSearchViewPropertiesType } from "../searchView.types";

const ajv = new Ajv({
  allErrors: true,
  verbose: true,
  discriminator: true,
  $data: true
});

/**
 * used to remove the schema on unmount, to avoid issues in next usage
 */
const useResetSchema = (
  schema: JSONSchemaType<ArangoSearchViewPropertiesType>
) => {
  useEffect(() => {
    return () => {
      ajv.removeSchema(schema.$id);
    };
  }, [schema]);
};
export const ArangoSearchJSONEditor = () => {
  const { values, setValues } = useFormikContext();
  const { initialView, setErrors, errors } = useEditViewContext();
  const { schema } = useArangoSearchJSONSchema({ view: initialView });
  useResetSchema(schema);
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
      <JSONErrors errors={errors} />
    </Box>
  );
};
