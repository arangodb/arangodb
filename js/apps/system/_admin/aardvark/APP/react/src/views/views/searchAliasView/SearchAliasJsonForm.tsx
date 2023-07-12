import { Box } from "@chakra-ui/react";
import Ajv from "ajv";
import { useFormikContext } from "formik";
import React from "react";
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

export const SearchAliasJsonForm = () => {
  const { setErrors, errors, setChanged } = useEditViewContext();
  const { setValues, values } =
    useFormikContext<SearchAliasViewPropertiesType>();
  const { schema } = useAliasViewSchema({ view: values });
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
