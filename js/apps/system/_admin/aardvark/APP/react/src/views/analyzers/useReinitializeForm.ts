import { useField, useFormikContext } from "formik";
import { AnalyzerState } from "./Analyzer.types";
import { useAnalyzersContext } from "./AnalyzersContext";
import { TYPE_TO_INITIAL_VALUES_MAP } from "./AnalyzersHelpers";

/**
 * This is used to reset initial values when the analyzer "type" changes.
 */

export const useReinitializeForm = () => {
  const { setValues, values } = useFormikContext<AnalyzerState>();
  const [typeField] = useField("type");
  const [featuresField] = useField("features");
  const { isFormDisabled: isDisabled } = useAnalyzersContext();
  const onReinitialize = () => {
    if (isDisabled) {
      return;
    }
    const initialValues =
      TYPE_TO_INITIAL_VALUES_MAP[
        typeField.value as keyof typeof TYPE_TO_INITIAL_VALUES_MAP
      ];
    const newValues = {
      ...initialValues,
      type: typeField.value,
      name: values?.name,
      // adding features which are already set
      features: featuresField.value
    };
    setValues(newValues);
  };
  return {
    onReinitialize
  };
};
