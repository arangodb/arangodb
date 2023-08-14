import { useFormikContext } from "formik";
import React from "react";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";

export const AnalyzerJSONForm = () => {
  const { values, setValues } = useFormikContext();
  return (
    <ControlledJSONEditor
      value={values}
      mode={"code"}
      onChange={updatedValues => {
        setValues(updatedValues);
      }}
      htmlElementProps={{
        style: {
          height: "calc(100vh - 300px)",
          width: "100%"
        }
      }}
    />
  );
};
