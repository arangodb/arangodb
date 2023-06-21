import { Global } from "@emotion/react";
import { JsonEditor } from "jsoneditor-react";
import React, { useEffect, useRef } from "react";

export const AQLEditor = ({
  defaultValue,
  onChange,
  isPreview
}: {
  defaultValue?: string;
  onChange: (value: string) => void;
  isPreview?: boolean;
}) => {
  const jsonEditorRef = useRef(null);
  useEffect(() => {
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    editor.options.onChange = () => onChange(editor.getText());
    editor.setText(defaultValue);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [jsonEditorRef]);
  useEffect(() => {
    if (!isPreview) {
      return;
    }
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    editor.setText(defaultValue);
    editor.aceEditor.setReadOnly(true);
  }, [isPreview, defaultValue]);
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
function useSetupAQLEditor(jsonEditorRef: React.MutableRefObject<null>) {
  useEffect(() => {
    const editor = (jsonEditorRef.current as any)?.jsonEditor;
    editor.options.mainMenuBar = false;

    const aceEditor = editor.aceEditor;
    aceEditor.$blockScrolling = Infinity;
    aceEditor.getSession().setMode("ace/mode/aql");
    aceEditor.setTheme("ace/theme/textmate");
    aceEditor.getSession().setUseWrapMode(true);
    aceEditor.setFontSize("14px");
    aceEditor.setShowPrintMargin(false);
  }, [jsonEditorRef]);
}
