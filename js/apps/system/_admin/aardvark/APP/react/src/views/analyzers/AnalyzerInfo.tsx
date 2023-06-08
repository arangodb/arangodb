import { AnalyzerDescription } from "arangojs/analyzer";
import { Formik } from "formik";
import React from "react";
import { AddAnalyzerForm } from "./AddAnalyzerForm";

export const AnalyzerInfo = ({
  analyzer
}: {
  analyzer: AnalyzerDescription;
}) => {
  return (
    <Formik
      initialValues={analyzer}
      onSubmit={() => {
        // noop
      }}
    >
      <AddAnalyzerForm />
    </Formik>
  );
};
