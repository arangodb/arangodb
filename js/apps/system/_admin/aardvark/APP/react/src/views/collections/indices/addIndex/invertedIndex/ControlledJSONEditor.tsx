import { JsonEditor, JsonEditorProps } from "jsoneditor-react";
import React, { useEffect, useRef } from "react";

export const ControlledJSONEditor = ({
  value,
  onChange,
  schema,
  ...rest
}: JsonEditorProps) => {
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
