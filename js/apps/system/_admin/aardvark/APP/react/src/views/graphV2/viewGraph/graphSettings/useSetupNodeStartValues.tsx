import { useEffect, useState } from "react";
import { OptionType } from "../../../../components/select/SelectBase";
import { useUrlParameterContext } from "../UrlParametersContext";

const convertNodeStartToValues = (nodeStart?: string) => {
  if (!nodeStart) {
    return [];
  }
  return nodeStart.split(" ").map(nodeId => {
    return { value: nodeId, label: nodeId };
  });
};

/**
 * Convert to a space separated string for backend.
 */
const convertValuesToNodeStartString = (values: OptionType[]) => {
  return values.map(value => value?.value).join(" ");
};

/**
 * Sets up values & handlers for the dropdown.
 */
export const useSetupNodeStartValues = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const initialNodeStartValues = convertNodeStartToValues(urlParams.nodeStart);
  // this ensures the values remain in sync
  // when the start node is set from the graph (right click menu)
  useEffect(() => {
    const nodeStartValues = convertNodeStartToValues(urlParams.nodeStart);
    setValues(nodeStartValues);
  }, [urlParams.nodeStart]);
  const [values, setValues] = useState<OptionType[]>(initialNodeStartValues);
  const onAddValue = (newValue?: OptionType) => {
    const updatedValues = newValue ? [...values, newValue] : values;
    const nodeStartString = convertValuesToNodeStartString(updatedValues);
    const newUrlParameters = {
      ...urlParams,
      nodeStart: nodeStartString || ""
    };
    setUrlParams(newUrlParameters);
    setValues(newValue ? updatedValues : []);
  };
  const onRemoveValue = (removedValue?: OptionType) => {
    const updatedValues = values.filter(value => {
      return value.value !== removedValue?.value;
    });
    const nodeStartString = convertValuesToNodeStartString(updatedValues);
    const newUrlParameters = {
      ...urlParams,
      // setting nodeStart to undefined here in order to remove value
      // (handled in useGraphSettingsHandlers)
      nodeStart: nodeStartString || undefined
    };
    setUrlParams(newUrlParameters);
    setValues(updatedValues);
  };
  return { values, setValues, onRemoveValue, onAddValue };
};
