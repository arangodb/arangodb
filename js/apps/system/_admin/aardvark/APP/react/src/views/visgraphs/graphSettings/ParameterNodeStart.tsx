import { FormLabel } from "@chakra-ui/react";
import React, { useState } from "react";
import MultiSelect from "../../../components/select/MultiSelect";
import { OptionType } from "../../../components/select/SelectBase";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useGraph } from "../GraphContext";
import { useUrlParameterContext } from "../UrlParametersContext";
import { useNodeStartOptions } from "./useNodeStartOptions";

const ParameterNodeStart = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();
  const nodeStartValue = urlParams.nodeStart
    ? urlParams.nodeStart.split(" ").map(nodeId => {
        return { value: nodeId, label: nodeId };
      })
    : [];
  const [values, setValues] = useState<OptionType[]>(nodeStartValue);
  const { graphName } = useGraph();

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
  const handleChange = (newValue?: OptionType) => {
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
  const [inputValue, setInputValue] = useState<string>("");
  const { options } = useNodeStartOptions({ graphName, inputValue });

  return (
    <>
      <FormLabel htmlFor="nodeStart">Start node</FormLabel>
      <MultiSelect
        noOptionsMessage={() => "No nodes found"}
        isClearable={false}
        styles={{
          container: baseStyles => {
            return { width: "240px", ...baseStyles };
          }
        }}
        inputId="nodeStart"
        value={values}
        options={options}
        inputValue={inputValue}
        placeholder="Enter 'collection_name/node_name'"
        onInputChange={(newValue, action) => {
          if (action.action === "set-value") {
            return;
          }
          setInputValue(newValue);
        }}
        closeMenuOnSelect={false}
        onChange={(_newValue, action) => {
          if (action.action === "select-option") {
            const [collectionName, vertexName] =
              action.option?.value.split("/") || [];
            if (collectionName && vertexName) {
              // this means a proper value is selected
              handleChange(action.option);
            }
            setInputValue(`${collectionName}/`);
          }

          if (
            action.action === "remove-value" ||
            action.action === "pop-value"
          ) {
            setInputValue("");
            onRemoveValue(action.removedValue);
            return;
          }
        }}
      />
      <InfoTooltip
        label={
          "A valid node ID or a space-separated list of IDs. If empty, a random node will be chosen."
        }
      />
    </>
  );
};

export default ParameterNodeStart;
