import { JsonEditor, JsonEditorProps } from "jsoneditor-react";
import React, { useEffect, useRef } from "react";

export const ControlledJSONEditor = ({
  value,
  onChange,
  ...rest
}: JsonEditorProps) => {
  const jsonEditorRef = useRef(null);
  useEffect(() => {
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    if (editor && value) {
      editor.set(value);
    }
  }, [jsonEditorRef, value]);

  return (
    <JsonEditor
      ref={jsonEditorRef}
      value={value}
      onChange={onChange}
      {...rest}
    />
  );
};
