import { OptionType } from "@arangodb/ui";
import { aql } from "arangojs";
import { useEffect, useState } from "react";
import { getCurrentDB } from "../../../../utils/arangoClient";

/**
 * fetches vertices or filters collections list
 * - Input value is of type `collectionName/vertexName`
 *   - If input value has no vertexName, this filters & returns collections list
 * - If input value contains `vertexName`, it makes an AQL query call to get vertices.
 */
const fetchOptions = async ({
  inputValue,
  collectionOptions,
  values
}: {
  inputValue: string;
  collectionOptions: OptionType[] | undefined;
  values: OptionType[];
}) => {
  const inputSplit = inputValue.split("/");
  const db = getCurrentDB();
  if (inputSplit.length === 1) {
    // filter here
    const finalOptions = inputValue
      ? collectionOptions?.filter(option =>
        option.value.includes(inputValue)
      ) || []
      : collectionOptions || [];
    return Promise.resolve(finalOptions);
  }
  const [collectionName, documentKey] = inputSplit;
  const valuesList = values
    .map(value => value.value.split("/"))
    .filter(valueSplit => valueSplit.length > 1 && valueSplit[0] === collectionName)
    .map(valueSplit => valueSplit[1]);
  const colQuery = aql`
      FOR doc IN ${db.collection(collectionName)}
      FILTER doc._key >= ${documentKey}
      FILTER doc._key NOT IN ${valuesList}
      SORT doc._key
      LIMIT 5
      FILTER STARTS_WITH(doc._key, ${documentKey}) 
      RETURN doc._id`;

  const cursor = await db.query(colQuery);
  const data = await cursor.all();
  if (!data) {
    return Promise.resolve([]);
  }
  return (data as string[]).map(value => {
    return {
      label: value,
      value: value
    };
  });
};

const useDebouncedValue = (value: string, delay: number) => {
  const [debouncedValue, setDebouncedValue] = useState(value);
  useEffect(() => {
    const timeout = setTimeout(() => setDebouncedValue(value), delay);
    return () => clearTimeout(timeout);
  }, [value, delay]);
  return debouncedValue;
};
/**
 * Takes the graph name and
 * the input entered by the user,
 * to fetches collections & vertices.
 *
 * Happens in two parts:
 * 1. Fetch all collections & perform client-side search in them.
 * 2. Search for vertices in that collection (via AQL)
 */

export const useNodeStartOptions = ({
  graphName,
  inputValue,
  values
}: {
  graphName: string;
  inputValue: string;
  values: OptionType[];
}) => {
  const [isLoading, setIsLoading] = useState(false);
  const debouncedInputValue = useDebouncedValue(inputValue, 500);
  const [vertexOptions, setVertexOptions] = useState<
    OptionType[] | undefined
  >();
  const [collectionOptions, setCollectionOptions] = useState<OptionType[]>();

  // loads collections present in the graph
  useEffect(() => {
    const db = getCurrentDB();
    const fetchGraphVertexCollection = async () => {
      let data = await db.graph(graphName).listVertexCollections();
      const collectionOptions = data.map(collectionName => {
        return {
          value: `${collectionName}/`,
          label: `${collectionName}/`
        };
      });
      setCollectionOptions(collectionOptions);
    };
    fetchGraphVertexCollection();
  }, [graphName]);
  useEffect(() => {
    setIsLoading(true);
  }, [inputValue]);
  // loads options based on inputValue
  useEffect(() => {
    const loadVertexOptions = async () => {
      const vertexOptions = await fetchOptions({
        inputValue: debouncedInputValue,
        collectionOptions,
        values
      });
      setVertexOptions(vertexOptions);
      setIsLoading(false);
    };
    loadVertexOptions();
  }, [debouncedInputValue, collectionOptions, values]);
  return { options: vertexOptions, isLoading };
};
