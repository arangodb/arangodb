import { JsonEditor, JsonEditorProps } from "jsoneditor-react";
import React, { useEffect, useRef } from "react";

/**
 * used to remove the schema on unmount, to avoid issues in next usage
 */
const useResetSchema = ({ schema, ajv }: { schema: any; ajv?: any }) => {
  useEffect(() => {
    return () => {
      ajv?.removeSchema(schema.$id);
    };
  }, [schema, ajv]);
};

export const ControlledJSONEditor = ({
  value,
  onChange,
  schema,
  ...rest
}: JsonEditorProps) => {
  useResetSchema({ schema, ajv: rest.ajv });
  const jsonEditorRef = useRef(null);
  useEffect(() => {
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    if (
      editor &&
      value &&
      JSON.stringify(value) !== JSON.stringify(editor.get())
    ) {
      editor.update(value);
    }
  }, [jsonEditorRef, value]);

  return (
    <JsonEditor
      schema={schema}
      ref={jsonEditorRef}
      value={value}
      onChange={onChange}
      {...rest}
    />
  );
};
