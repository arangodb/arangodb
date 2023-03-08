import { Box } from "@chakra-ui/react";
import Ajv, { JSONSchemaType } from "ajv";
import { JsonEditor } from "jsoneditor-react";
import React, { useEffect, useRef } from "react";
import { useSearchAliasContext } from "./SearchAliasContext";
import { useAliasViewSchema } from "./SearchAliasJsonHelper";
import { ViewPropertiesType } from "./useFetchViewProperties";

const ajv = new Ajv({
  allErrors: true,
  verbose: true,
  discriminator: true,
  $data: true
});

const useResetSchema = (schema: JSONSchemaType<ViewPropertiesType>) => {
  useEffect(() => {
    return () => {
      ajv.removeSchema(schema.$id);
    };
  }, [schema]);
};

export const SearchAliasJsonForm = () => {
  const { view, onChange, setErrors } = useSearchAliasContext();
  const { schema } = useAliasViewSchema({ view });
  useResetSchema(schema);
  const jsonEditorRef = useRef(null);
  return (
    <Box>
      <JsonEditor
        ref={jsonEditorRef}
        ajv={ajv}
        value={view}
        onChange={json => {
          onChange(json);
        }}
        schema={schema}
        onValidationError={errors => {
          setErrors(errors);
        }}
        mode={"code"}
        history={true}
        htmlElementProps={{
          style: {
            height: "calc(100% - 40px)"
          }
        }}
      />
      <JsonErrors />
    </Box>
  );
};

const JsonErrors = () => {
  const { errors } = useSearchAliasContext();
  if (errors.length === 0) {
    return null;
  }
  return (
    <Box
      maxHeight={"40px"}
      paddingX="2"
      paddingY="1"
      fontSize="sm"
      color="red"
      background="red.100"
      overflow={"auto"}
    >
      {errors.map(error => {
        return <Box>{error.message}</Box>;
      })}
    </Box>
  );
};
