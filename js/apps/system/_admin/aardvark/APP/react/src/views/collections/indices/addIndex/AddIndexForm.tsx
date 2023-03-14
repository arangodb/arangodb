import { Box, Stack } from "@chakra-ui/react";
import React, { useState } from "react";
import SingleSelect from "../../../../components/select/SingleSelect";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";
import { FulltextIndexForm } from "./FulltextIndexForm";
import { PersistentIndexForm } from "./PersistentIndexForm";

export const AddIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { indexTypeOptions } = useCollectionIndicesContext();
  const [indexType, setIndexType] = useState(indexTypeOptions?.[0].value || "");
  return (
    <Box width="100%" padding="4">
      <Stack padding="4" background="white" spacing="4">
        <Box display={"grid"} gridTemplateColumns={"200px 1fr"} rowGap="5">
          <Box fontSize={"lg"}>Add new index</Box>
          <SingleSelect
            defaultValue={indexTypeOptions?.[0]}
            options={indexTypeOptions}
            onChange={value => {
              setIndexType((value as any).value);
            }}
          />
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
  type: string;
  onClose: () => void;
}) => {
  if (type === "persistent") {
    return <PersistentIndexForm onClose={onClose} />;
  }
  if (type === "fulltext") {
    return <FulltextIndexForm onClose={onClose} />;
  }
  if (type === "skiplist") {
    return <FulltextIndexForm onClose={onClose} />;
  }
  if (type === "hash") {
    return <FulltextIndexForm onClose={onClose} />;
  }
  if (type === "ttl") {
    return <FulltextIndexForm onClose={onClose} />;
  }
  if (type === "geo") {
    return <FulltextIndexForm onClose={onClose} />;
  }
  if (type === "zkd") {
    return <FulltextIndexForm onClose={onClose} />;
  }
  return <></>;
};
