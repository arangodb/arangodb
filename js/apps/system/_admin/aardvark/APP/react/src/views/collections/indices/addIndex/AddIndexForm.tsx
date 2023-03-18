import { Box } from "@chakra-ui/react";
import React, { useState } from "react";
import SingleSelect from "../../../../components/select/SingleSelect";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";
import { FulltextIndexForm } from "./FulltextIndexForm";
import { GeoIndexForm } from "./GeoIndexForm";
import { InfoTooltip } from "./InfoTooltip";
import { InvertedIndexForm } from "./InvertedIndexForm";
import { PersistentIndexForm } from "./PersistentIndexForm";
import { TTLIndexForm } from "./TTLIndexForm";
import { ZKDIndexForm } from "./ZKDIndexForm";

export const AddIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { indexTypeOptions } = useCollectionIndicesContext();
  const [indexType, setIndexType] = useState(indexTypeOptions?.[0].value || "");
  let tooltipText = "Type of index to create.";
  if (!indexTypeOptions?.find(option => option.value === "hash")) {
    tooltipText = `${tooltipText} Please note that for the RocksDB engine the index types "hash", "skiplist' and "persistent" are identical, so that they are not offered seperately here.`;
  }
  return (
    <Box width="100%" padding="4" height="full" background="white">
      <Box
        display={"grid"}
        gridTemplateColumns={"200px 1fr 40px"}
        rowGap="5"
        columnGap="3"
        maxWidth="800px"
      >
        <Box fontSize={"lg"}>Add new index</Box>
        <SingleSelect
          defaultValue={indexTypeOptions?.[0]}
          options={indexTypeOptions}
          onChange={value => {
            setIndexType((value as any).value);
          }}
        />
        <InfoTooltip label={tooltipText} />
      </Box>
      <Box height="calc(100% - 48px)" marginTop="2">
        <IndexTypeForm onClose={onClose} type={indexType} />
      </Box>
    </Box>
  );
};

const IndexTypeForm = ({
  type,
  onClose
}: {
  type: string;
  onClose: () => void;
}) => {
  if (type === "inverted") {
    return <InvertedIndexForm onClose={onClose} />;
  }
  if (type === "persistent") {
    return <PersistentIndexForm onClose={onClose} />;
  }
  if (type === "fulltext") {
    return <FulltextIndexForm onClose={onClose} />;
  }
  if (type === "ttl") {
    return <TTLIndexForm onClose={onClose} />;
  }
  if (type === "geo") {
    return <GeoIndexForm onClose={onClose} />;
  }
  if (type === "zkd") {
    return <ZKDIndexForm onClose={onClose} />;
  }
  return <></>;
};
