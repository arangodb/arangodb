import { ClassNames } from "@emotion/react";
import { JsonEditor, JsonEditorProps } from "jsoneditor-react";
import React, { useEffect, useRef } from "react";

export const ControlledJSONEditor = ({
  value,
  onChange,
  schema,
  isDisabled,
  htmlElementProps,
  ...rest
}: JsonEditorProps & {
  isDisabled?: boolean;
}) => {
  useEffect(() => {
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    const ace = editor?.aceEditor;
    if (!editor || !ace) return;
    if (isDisabled) {
      ace.setReadOnly(true);
    } else {
      ace.setReadOnly(false);
    }
  }, [isDisabled]);
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
    <ClassNames>
      {({ css }) => (
        <JsonEditor
          schema={schema}
          ref={jsonEditorRef}
          value={value}
          onChange={onChange}
          htmlElementProps={{
            ...htmlElementProps,
            className: isDisabled
              ? css`
                  opacity: 0.6;
                `
              : ""
          }}
          {...rest}
        />
      )}
    </ClassNames>
  );
};
