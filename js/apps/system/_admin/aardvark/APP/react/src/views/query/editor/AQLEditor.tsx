import { Global } from "@emotion/react";
import { JsonEditor } from "jsoneditor-react";
import React, { useEffect, useRef } from "react";

export const AQLEditor = ({
  value,
  onChange,
  isPreview,
  autoFocus,
  resetEditor
}: {
  value?: string;
  onChange?: (value: string) => void;
  isPreview?: boolean;
  resetEditor?: boolean;
  autoFocus?: boolean;
}) => {
  const jsonEditorRef = useRef(null);

  useEffect(() => {
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    editor.options.onChangeText = (value: string) => {
      onChange?.(value);
    };

    /**
     * directly call aceEditor.setValue to avoid cursor jump.
     * This can't be called on every value change because that
     * will destroy the undo stack
     */
    editor.aceEditor.setValue(value, 1);
    /**
     * disabled because we are using the value as 'defaultValue',
     * and onChange function update can be ignored
     * */
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [resetEditor]);

  /** set to readOnly when in preview mode */
  useEffect(() => {
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    if (isPreview) {
      editor.aceEditor.setReadOnly(true);
      editor.aceEditor.setValue(value, 1);
    }
  }, [isPreview, value]);

  useEffect(() => {
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    if (autoFocus) {
      editor.aceEditor.focus();
    }
  }, [autoFocus]);
  useSetupAQLEditor(jsonEditorRef);
  return (
    <>
      <Global
        styles={{
          ".jsoneditor div.jsoneditor-outer.has-status-bar": {
            padding: "0px",
            marginTop: "0px"
          }
        }}
      />
      <JsonEditor
        ref={jsonEditorRef}
        mode={"code" as any}
        onChange={onChange}
        value={undefined}
        htmlElementProps={{
          style: {
            height: "100%",
            width: "100%"
          }
        }}
        // @ts-ignore
        mainMenuBar={false}
      />
    </>
  );
};

const useSetupAQLEditor = (jsonEditorRef: React.MutableRefObject<null>) => {
  useEffect(() => {
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    const aceEditor = editor.aceEditor;
    aceEditor.$blockScrolling = Infinity;
    aceEditor.getSession().setMode("ace/mode/aql");
    aceEditor.setTheme("ace/theme/textmate");
    aceEditor.getSession().setUseWrapMode(true);
    aceEditor.setFontSize("10pt");
    aceEditor.setShowPrintMargin(false);
  }, [jsonEditorRef]);
};
