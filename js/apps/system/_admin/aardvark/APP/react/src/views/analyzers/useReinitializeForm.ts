import { useField, useFormikContext } from "formik";
import { useEffect } from "react";
import { AnalyzerState } from "./Analyzer.types";
import { useAnalyzersContext } from "./AnalyzersContext";
import { TYPE_TO_INITIAL_VALUES_MAP } from "./AnalyzersHelpers";

/**
 * This is used to reset initial values when the analyzer "type" changes.
 */

export const useReinitializeForm = () => {
  const { setValues, values } = useFormikContext<AnalyzerState>();
  const [field] = useField("type");
  const [featuresField] = useField("features");
  const { isFormDisabled: isDisabled } = useAnalyzersContext();

  useEffect(() => {
    if (isDisabled) {
      return;
    }
    const newValues = {
      ...TYPE_TO_INITIAL_VALUES_MAP[
        field.value as keyof typeof TYPE_TO_INITIAL_VALUES_MAP
      ],
      name: values.name,
      // adding features which are already set
      features: featuresField.value
    };
    setValues(newValues);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [field.value, isDisabled]);
};
