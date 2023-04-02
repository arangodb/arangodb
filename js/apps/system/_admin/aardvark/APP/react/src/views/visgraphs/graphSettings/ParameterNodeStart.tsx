import { FormLabel } from "@chakra-ui/react";
import React, { useEffect, useState } from "react";
import AsyncMultiSelect from "../../../components/select/AsyncMultiSelect";
import { OptionType } from "../../../components/select/SelectBase";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { getCurrentDB } from "../../../utils/arangoClient";
import { useGraph } from "../GraphContext";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeStart = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();
  const nodeOptions = urlParams.nodeStart
    ? urlParams.nodeStart.split(" ").map(nodeId => {
        return { value: nodeId, label: nodeId };
      })
    : [];
  const [values, setValue] = useState<OptionType[]>(nodeOptions);
  const { graphName } = useGraph();

  const updateUrlParamsForNode = (updatedValues: OptionType[]) => {
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
    setValue(newValue ? updatedValues : []);
  };
  const removeValue = (removedValue?: OptionType) => {
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
    setValue(updatedValues);
  };
  const [graphVertexCollections, setGraphVertexCollections] = useState<
    string[]
  >([]);
  const [inputValue, setInputValue] = useState<string>("");
  const [initialOptions, setInitialOptions] = useState<OptionType[]>();
  useEffect(() => {
    const db = getCurrentDB();
    const fetchGraphVertexCollection = async () => {
      let data = await db.graph(graphName).listVertexCollections();
      const initialOptions = data.map(collectionName => {
        return {
          value: `${collectionName}/`,
          label: `${collectionName}/`
        };
      });
      setInitialOptions(initialOptions);
      setGraphVertexCollections(data);
    };
    fetchGraphVertexCollection();
  }, [graphName]);

  const loadOptions = async (inputValue: string) => {
    const inputSplit = inputValue.split("/");
    const [collectionName] = inputSplit;
    const db = getCurrentDB();
    if (inputSplit.length === 1) {
      // filter here
      return Promise.resolve(
        initialOptions?.filter(option => option.value.includes(inputValue)) ||
          []
      );
    }
    let newCollections = [...graphVertexCollections];
    if (collectionName) {
      newCollections = [collectionName];
    }
    let queries: string[] = [];
    newCollections.forEach(collection => {
      const colQuery =
        "(FOR doc IN " +
        collection +
        " FILTER " +
        "doc._id >= @search && CONTAINS(doc._id, @search) " +
        "LIMIT 5 " +
        "RETURN doc._id)";
      queries = [...queries, colQuery];
    });
    const cursor = await db.query(
      "RETURN FLATTEN(APPEND([], [" + queries.join(", ") + "]))",
      { search: inputValue }
    );
    const data = await cursor.all();
    if (!data) {
      return Promise.resolve([]);
    }
    return (data[0] as string[]).map(value => {
      return {
        label: value,
        value: value
      };
    });
  };

  return (
    <>
      <FormLabel htmlFor="nodeStart">Start node</FormLabel>
      <AsyncMultiSelect
        isClearable={false}
        styles={{
          container: baseStyles => {
            return { width: "240px", ...baseStyles };
          }
        }}
        id="nodeStart"
        value={values}
        defaultOptions={initialOptions}
        inputValue={inputValue}
        placeholder="Enter 'collection_name/node_name'"
        loadOptions={loadOptions}
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
              handleChange(action.option);
            }
            setInputValue(`${collectionName}/`);
          }

          if (
            action.action === "remove-value" ||
            action.action === "pop-value"
          ) {
            setInputValue("");
            removeValue(action.removedValue);
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
