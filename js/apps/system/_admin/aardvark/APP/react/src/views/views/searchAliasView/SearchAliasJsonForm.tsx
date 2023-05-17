import { Box } from "@chakra-ui/react";
import Ajv, { JSONSchemaType } from "ajv";
import { JsonEditor } from "jsoneditor-react";
import React, { useEffect, useRef } from "react";
import { SearchAliasViewPropertiesType } from "../searchView.types";
import { useSearchAliasContext } from "./SearchAliasContext";
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
  const {
    view,
    copiedView,
    onChange,
    setErrors,
    setCopiedView,
    setView,
    currentName,
    setCurrentName
  } = useSearchAliasContext();
  const { schema } = useAliasViewSchema({ view });
  useResetSchema(schema);
  const jsonEditorRef = useRef(null);

  useEffect(() => {
    const currentData = (jsonEditorRef.current as any)?.jsonEditor.get();
    if (copiedView && currentData !== copiedView) {
      (jsonEditorRef.current as any)?.jsonEditor.set(copiedView);
      setView(copiedView);
      setCopiedView(undefined);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [copiedView]);
  useEffect(() => {
    const currentData = (jsonEditorRef.current as any)?.jsonEditor.get();
    if (currentData.name !== currentName) {
      const newView = { ...currentData, name: currentName || view.name };
      setView(newView);
      (jsonEditorRef.current as any)?.jsonEditor.set(newView);
      setCurrentName(undefined);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [currentName]);
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
        return (
          <Box>{`${error.keyword} error: ${
            error.message
          }. Schema: ${JSON.stringify(error.params)}`}</Box>
        );
      })}
    </Box>
  );
};
