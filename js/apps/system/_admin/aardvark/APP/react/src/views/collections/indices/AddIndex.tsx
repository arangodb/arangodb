import { Box, Stack } from "@chakra-ui/react";
import React, { useState } from "react";
import SingleSelect from "../../../components/select/SingleSelect";
import { useCollectionIndicesContext } from "./CollectionIndicesContext";
import { FulltextIndexForm } from "./FulltextIndexForm";
import { PersistentIndexForm } from "./PersistentIndexForm";

export const AddIndex = ({ onClose }: { onClose: () => void }) => {
  const [indexType, setIndexType] = useState("");
  const { indexTypeOptions } = useCollectionIndicesContext();
  return (
    <Box width="100%" padding="4">
      <Stack padding="4" background="white" spacing="4">
        <Box display={"grid"} gridTemplateColumns={"200px 1fr"} rowGap="5">
          <Box fontSize={"lg"}>Add new index</Box>
          <SingleSelect
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
  if (type === "fulltext") {
    return <FulltextIndexForm onClose={onClose} />;
  }
  if (type === "persistent") {
    return <PersistentIndexForm onClose={onClose} />;
  }
  return <></>;
};
