import { Box } from "@chakra-ui/react";
import Ajv, { JSONSchemaType } from "ajv";
import { useFormikContext } from "formik";
import React, { useEffect } from "react";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { JSONErrors } from "../../../components/jsonEditor/JSONErrors";
import { useEditViewContext } from "../editView/EditViewContext";
import { SearchAliasViewPropertiesType } from "../searchView.types";
import { useAliasViewSchema } from "./SearchAliasJsonHelper";

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
  schema: JSONSchemaType<SearchAliasViewPropertiesType>
) => {
  useEffect(() => {
    return () => {
      ajv.removeSchema(schema.$id);
    };
  }, [schema]);
};

export const SearchAliasJsonForm = () => {
  const { setErrors, errors, setChanged } = useEditViewContext();
  const { setValues, values } =
    useFormikContext<SearchAliasViewPropertiesType>();
  const { schema } = useAliasViewSchema({ view: values });
  useResetSchema(schema);
  return (
    <Box>
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
            setChanged(true);
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
