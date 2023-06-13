import { useFormikContext } from "formik";
import React from "react";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";

export const AnalyzerJSONForm = () => {
  const { values } = useFormikContext();
  return (
    <ControlledJSONEditor
      value={values}
      mode={"code"}
      htmlElementProps={{
        style: {
          height: "calc(100vh - 300px)",
          width: "100%"
        }
      }}
    />
  );
};
