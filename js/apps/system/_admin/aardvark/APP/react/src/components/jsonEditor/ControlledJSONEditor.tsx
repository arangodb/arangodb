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

type ControlledJSONEditorProps = Omit<JsonEditorProps, "value"> & {
  value?: any;
  isDisabled?: boolean;
  mainMenuBar?: boolean;
  isReadOnly?: boolean;
  defaultValue?: any;
};

export const ControlledJSONEditor = React.forwardRef(
  (
    {
      value,
      onChange,
      schema,
      isDisabled,
      isReadOnly,
      htmlElementProps,
      mainMenuBar,
      defaultValue,
      ...rest
    }: ControlledJSONEditorProps,
    ref: React.ForwardedRef<JsonEditor>
  ) => {
    const jsonEditorRef = useRef<JsonEditor | undefined>();
    useResetSchema({ schema, ajv: rest.ajv });
    useSetupReadOnly({ jsonEditorRef, isDisabled, isReadOnly });
    useSetupValueUpdate({ jsonEditorRef, value });
    useSetupDefaultValueUpdate({ jsonEditorRef, defaultValue });
    return (
      <ClassNames>
        {({ css }) => (
          <JsonEditor
            schema={schema}
            ref={node => {
              if (node) {
                jsonEditorRef.current = node;
              }
              if (typeof ref === "function") {
                ref(node);
              } else if (ref) {
                ref.current = node;
              }
            }}
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
  }
);

const useSetupReadOnly = ({
  jsonEditorRef,
  isDisabled,
  isReadOnly
}: {
  jsonEditorRef?: React.MutableRefObject<JsonEditor | undefined>;
  isDisabled: boolean | undefined;
  isReadOnly: boolean | undefined;
}) => {
  useEffect(() => {
    const editor = (jsonEditorRef?.current as any)?.jsonEditor;
    const ace = editor?.aceEditor;
    if (!editor || !ace) return;
    if (isDisabled || isReadOnly) {
      ace.setReadOnly(true);
    } else {
      ace.setReadOnly(false);
    }
  }, [isDisabled, isReadOnly, jsonEditorRef]);
};

const useSetupDefaultValueUpdate = ({
  jsonEditorRef,
  defaultValue
}: {
  jsonEditorRef?: React.MutableRefObject<JsonEditor | undefined>;
  defaultValue: any;
}) => {
  useEffect(() => {
    const editor = (jsonEditorRef?.current as any)?.jsonEditor;
    if (editor && defaultValue) {
      editor.update(defaultValue);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [jsonEditorRef]);
};

const useSetupValueUpdate = ({
  jsonEditorRef,
  value
}: {
  jsonEditorRef: React.MutableRefObject<JsonEditor | undefined>;
  value: any;
}) => {
  useEffect(() => {
    const editor = (jsonEditorRef?.current as any)?.jsonEditor;
    if (
      editor &&
      value &&
      JSON.stringify(value) !== JSON.stringify(editor.get())
    ) {
      console.log({ value });
      editor.update(value);
    }
  }, [jsonEditorRef, value]);
};
