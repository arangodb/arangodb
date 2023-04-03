import { useEffect, useState } from "react";
import { OptionType } from "../../../components/select/SelectBase";
import { getCurrentDB } from "../../../utils/arangoClient";

const fetchVertexOptions = async ({
  inputValue,
  collectionOptions
}: {
  inputValue: string;
  collectionOptions: OptionType[] | undefined;
}) => {
  const inputSplit = inputValue.split("/");
  const [collectionName] = inputSplit;
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
export const useNodeStartOptions = ({
  graphName,
  inputValue
}: {
  graphName: string;
  inputValue: string;
}) => {
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

  // loads options based on inputValue
  useEffect(() => {
    const loadVertexOptions = async () => {
      const vertexOptions = await fetchVertexOptions({
        inputValue,
        collectionOptions
      });
      setVertexOptions(vertexOptions);
    };
    loadVertexOptions();
  }, [inputValue, collectionOptions]);
  return { options: vertexOptions };
};
