import { useEffect, useState } from "react";
import { OptionType } from "../../../components/select/SelectBase";
import { useUrlParameterContext } from "../UrlParametersContext";

export const useSetupNodeStartValues = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const initialNodeStartValues = urlParams.nodeStart
    ? urlParams.nodeStart.split(" ").map(nodeId => {
        return { value: nodeId, label: nodeId };
      })
    : [];
  useEffect(() => {
    // this ensures sync when the start node is set from the graph
    const nodeStartValues = urlParams.nodeStart
      ? urlParams.nodeStart.split(" ").map(nodeId => {
          return { value: nodeId, label: nodeId };
        })
      : [];
    setValues(nodeStartValues);
  }, [urlParams.nodeStart]);
  const [values, setValues] = useState<OptionType[]>(initialNodeStartValues);
  const updateUrlParamsForNode = (updatedValues: OptionType[]) => {
    // convert to a space separated string for backend
    const nodeStartString =
      updatedValues.map(value => value?.value).join(" ") || "";
    const newUrlParameters = {
      ...urlParams,
      nodeStart: nodeStartString || ""
    };
    setUrlParams(newUrlParameters);
  };
  const onAddValue = (newValue?: OptionType) => {
    const updatedValues = newValue ? [...values, newValue] : values;
    updateUrlParamsForNode(updatedValues);
    setValues(newValue ? updatedValues : []);
  };
  const onRemoveValue = (removedValue?: OptionType) => {
    const updatedValues = values.filter(value => {
      return value.value !== removedValue?.value;
    });
    const nodeStartString =
      updatedValues.map(value => value?.value).join(" ") || "";
    const newUrlParameters = {
      ...urlParams,
      nodeStart: nodeStartString || undefined
    };
    setUrlParams(newUrlParameters);
    setValues(updatedValues);
  };
  return { values, setValues, onRemoveValue, onAddValue };
};
