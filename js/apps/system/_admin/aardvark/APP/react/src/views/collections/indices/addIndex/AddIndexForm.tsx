import { Box, FormLabel } from "@chakra-ui/react";
import { Index } from "arangojs/indexes";
import React, { useState } from "react";
import SingleSelect from "../../../../components/select/SingleSelect";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";
import { FulltextIndexForm } from "./FulltextIndexForm";
import { GeoIndexForm } from "./GeoIndexForm";
import { IndexInfoTooltip } from "./IndexInfoTooltip";
import { InvertedIndexFormWrap } from "./invertedIndex/InvertedIndexFormWrap";
import { PersistentIndexForm } from "./PersistentIndexForm";
import { TTLIndexForm } from "./TTLIndexForm";
import { ZKDIndexForm } from "./ZKDIndexForm";

export const AddIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { indexTypeOptions } = useCollectionIndicesContext();
  const [indexType, setIndexType] = useState<Index["type"]>(
    indexTypeOptions[0].value
  );
  let tooltipText = "Type of index to create.";
  return (
    <Box width="100%" paddingY="4" height="full" background="white">
      <Box fontSize={"lg"} paddingX="10">
        Add new index
      </Box>
      <Box
        display={"grid"}
        gridTemplateColumns={"200px 1fr 40px"}
        rowGap="5"
        columnGap="3"
        maxWidth="800px"
        marginTop="4"
        paddingRight="8"
        paddingLeft="10"
      >
        <FormLabel htmlFor="type">Type</FormLabel>
        <SingleSelect
          inputId="type"
          defaultValue={indexTypeOptions[0]}
          options={indexTypeOptions}
          onChange={value => {
            setIndexType(value?.value as Index["type"]);
          }}
        />
        <IndexInfoTooltip label={tooltipText} />
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
  type: Index["type"];
  onClose: () => void;
}) => {
  if (type === "inverted") {
    return <InvertedIndexFormWrap onClose={onClose} />;
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
