import { FormLabel } from "@chakra-ui/react";
import React, { useEffect, useState } from "react";
import MultiSelect from "../../../components/select/MultiSelect";
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
  const [inputValue, setInputValue] = useState<string>("");
  const [initialOptions, setInitialOptions] = useState<OptionType[]>();
  const [options, setOptions] = useState<OptionType[] | undefined>();
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
    };
    fetchGraphVertexCollection();
  }, [graphName]);

  useEffect(() => {
    const loadOptions = async () => {
      const options = await fetchOptions({ inputValue, initialOptions });
      setOptions(options);
    };
    loadOptions();
  }, [inputValue, initialOptions]);

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
        id="nodeStart"
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

const fetchOptions = async ({
  inputValue,
  initialOptions
}: {
  inputValue: string;
  initialOptions: OptionType[] | undefined;
}) => {
  const inputSplit = inputValue.split("/");
  const [collectionName] = inputSplit;
  const db = getCurrentDB();
  if (inputSplit.length === 1) {
    // filter here
    const finalOptions = inputValue
      ? initialOptions?.filter(option => option.value.includes(inputValue)) ||
        []
      : initialOptions || [];
    return Promise.resolve(finalOptions);
  }
  const newCollections = [collectionName];
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
