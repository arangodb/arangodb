import { Box, Stack } from "@chakra-ui/react";
import React, { useState } from "react";
import SingleSelect from "../../../../components/select/SingleSelect";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";
import { IndexType } from "../useFetchIndices";
import { FulltextIndexForm } from "./FulltextIndexForm";
import { GeoIndexForm } from "./GeoIndexForm";
import { InfoTooltip } from "./InfoTooltip";
import { PersistentIndexForm } from "./PersistentIndexForm";
import { TTLIndexForm } from "./TTLIndexForm";
import { ZKDIndexForm } from "./ZKDIndexForm";

export const AddIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { indexTypeOptions } = useCollectionIndicesContext();
  const [indexType, setIndexType] = useState<IndexType>(
    indexTypeOptions[0].value
  );
  let tooltipText = "Type of index to create.";
  if (!indexTypeOptions?.find(option => option.value === "hash")) {
    tooltipText = `${tooltipText} Please note that for the RocksDB engine the index types "hash", "skiplist" and "persistent" are identical, so that they are not offered separately here.`;
  }
  return (
    <Box width="100%" padding="4">
      <Stack padding="4" background="white" spacing="4">
        <Box
          display={"grid"}
          gridTemplateColumns={"200px 1fr 40px"}
          rowGap="5"
          columnGap="3"
          maxWidth="500px"
        >
          <Box fontSize={"lg"}>Add new index</Box>
          <SingleSelect
            defaultValue={indexTypeOptions[0]}
            options={indexTypeOptions}
            onChange={value => {
              if (value?.value) {
                setIndexType(value.value as IndexType);
              }
            }}
          />
          <InfoTooltip label={tooltipText} />
        </Box>
        <IndexTypeForm onClose={onClose} type={indexType} />
      </Stack>
    </Box>
  );
};

const IndexTypeForm = ({
  type,
  onClose
}: {
  type: IndexType;
  onClose: () => void;
}) => {
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
