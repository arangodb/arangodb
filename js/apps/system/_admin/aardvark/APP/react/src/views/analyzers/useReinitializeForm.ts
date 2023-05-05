import { AnalyzerDescription } from "arangojs/analyzer";
import { useField } from "formik";
import { useEffect } from "react";
import { useAnalyzersContext } from "./AnalyzersContext";
import { TYPE_TO_INITIAL_VALUES_MAP } from "./AnalyzersHelpers";

/**
 * This is used to reset initial values when the analyzer "type" changes.
 */

export const useReinitializeForm = () => {
  const { setInitialValues } = useAnalyzersContext();
  const [field] = useField("type");
  const [featuresField] = useField("features");
  useEffect(() => {
    const initialValues = {
      ...TYPE_TO_INITIAL_VALUES_MAP[
        field.value as keyof typeof TYPE_TO_INITIAL_VALUES_MAP
      ],
      // adding features which are already set
      features: featuresField.value
    };
    // todo - remove after arangojs types are updated
    setInitialValues(initialValues as AnalyzerDescription);

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [field.value]);
};
