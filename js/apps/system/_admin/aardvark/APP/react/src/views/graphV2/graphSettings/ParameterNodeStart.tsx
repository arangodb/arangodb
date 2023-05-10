import { FormLabel } from "@chakra-ui/react";
import React, { useState } from "react";
import MultiSelect from "../../../components/select/MultiSelect";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useGraph } from "../GraphContext";
import { useNodeStartOptions } from "./useNodeStartOptions";
import { useSetupNodeStartValues } from "./useSetupNodeStartValues";

const ParameterNodeStart = () => {
  const { graphName } = useGraph();
  const [inputValue, setInputValue] = useState<string>("");

  const { values, onRemoveValue, onAddValue } = useSetupNodeStartValues();
  const { options } = useNodeStartOptions({ graphName, inputValue, values });

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
            // when a value is being set (by clicking enter), don't re-fill the input
            return;
          }
          setInputValue(newValue.normalize());
        }}
        closeMenuOnSelect={false}
        onChange={(_newValue, action) => {
          if (action.action === "select-option") {
            const [collectionName, vertexName] =
              action.option?.value.split("/") || [];
            if (collectionName && vertexName) {
              // this means a proper value is selected
              onAddValue(action.option);
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
