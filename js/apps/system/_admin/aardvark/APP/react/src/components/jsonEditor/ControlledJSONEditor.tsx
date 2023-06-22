import { ClassNames } from "@emotion/react";
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
  isDisabled,
  isReadOnly,
  htmlElementProps,
  mainMenuBar,
  defaultValue,
  ...rest
}: Omit<JsonEditorProps, "value"> & {
  value?: any;
  isDisabled?: boolean;
  mainMenuBar?: boolean;
  isReadOnly?: boolean;
  defaultValue?: any;
}) => {
  useEffect(() => {
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    const ace = editor?.aceEditor;
    if (!editor || !ace) return;
    if (isDisabled || isReadOnly) {
      ace.setReadOnly(true);
    } else {
      ace.setReadOnly(false);
    }
  }, [isDisabled, isReadOnly]);
  useResetSchema({ schema, ajv: rest.ajv });
  const jsonEditorRef = useRef(null);
  useEffect(() => {
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    if (
      editor &&
      value &&
      JSON.stringify(value) !== JSON.stringify(editor.get())
    ) {
      console.log({ value });
      editor.update(value);
    }
  }, [jsonEditorRef, value]);
  useEffect(() => {
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    if (editor && defaultValue) {
      editor.update(defaultValue);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [jsonEditorRef]);
  return (
    <ClassNames>
      {({ css }) => (
        <JsonEditor
          schema={schema}
          ref={jsonEditorRef}
          value={value || {}}
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
          // @ts-ignore
          mainMenuBar={mainMenuBar}
        />
      )}
    </ClassNames>
  );
};
