import { Global } from "@emotion/react";
import { JsonEditor } from "jsoneditor-react";
import React, { useEffect, useRef } from "react";

export const AQLEditor = ({
  value,
  onChange,
  isPreview,
  resetEditor
}: {
  value?: string;
  onChange?: (value: string) => void;
  isPreview?: boolean;
  resetEditor?: boolean;
}) => {
  const jsonEditorRef = useRef(null);
  useEffect(() => {
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    editor.options.onChangeText = (value: string) => {
      onChange?.(value);
    };
    editor.updateText(value);
    // disabled because onChange updates can be ignored
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [resetEditor]);

  /** set to readOnly when in preview mode */
  useEffect(() => {
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    if (isPreview) {
      editor.aceEditor.setReadOnly(true);
      editor.updateText(value);
    }
  }, [isPreview, value]);
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
    aceEditor.setFontSize("14px");
    aceEditor.setShowPrintMargin(false);
  }, [jsonEditorRef]);
};
