import { Box, Stack } from "@chakra-ui/react";
import React, { useState } from "react";
import SingleSelect from "../../../components/select/SingleSelect";
import { FulltextIndexForm } from "./FulltextIndexForm";

const indexTypeOptions = [
  {
    label: "Persistent Index",
    value: "persistent"
  },
  {
    label: "Geo Index",
    value: "geo"
  },
  {
    label: "Fulltext Index",
    value: "fulltext"
  },
  {
    label: "TTL Index",
    value: "ttl"
  },
  {
    label: "ZKD Index (EXPERIMENTAL)",
    value: "zdk"
  }
];
export const AddIndex = ({ onClose }: { onClose: () => void }) => {
  const [indexType, setIndexType] = useState("");
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
  return <></>;
};
