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
    const initialProperties = (initialValues as any)?.properties || {};
    const newProperties = (values as any)?.properties || {};
    // merging the new properties with the old ones
    const initialPropertiesKeys = Object.keys(initialProperties);
    const valuesPropertiesKeys = Object.keys(newProperties);
    const mergedProperties = valuesPropertiesKeys.reduce((acc, key) => {
      if (initialPropertiesKeys.includes(key)) {
        (acc as any)[key] = (values as any)?.properties[key];
      }
      return acc;
    }, {});
    const newValues = {
      ...initialValues,
      properties: {
        ...initialProperties,
        ...mergedProperties
      },
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
