import { Flex, Stat, StatLabel, StatNumber } from "@chakra-ui/react";
import { HiddenIndex } from "arangojs/indexes";
import React from "react";

export const IndexStats = ({
  foundCollectionIndex
}: {
  foundCollectionIndex: HiddenIndex;
}) => {
  const { figures, type } = foundCollectionIndex;

  if (type === "inverted") {
    const {
      indexSize,
      numDocs,
      numFiles,
      numLiveDocs,
      numPrimaryDocs,
      numSegments
    } = figures || {};
    return (
      <Flex>
        {indexSize ? (
          <Stat>
            <StatNumber>{window.prettyBytes(indexSize)}</StatNumber>
            <StatLabel>Index size</StatLabel>
          </Stat>
        ) : null}
        {numDocs ? (
          <Stat>
            <StatNumber>{numDocs}</StatNumber>
            <StatLabel>Number of docs</StatLabel>
          </Stat>
        ) : null}
        {numFiles ? (
          <Stat>
            <StatNumber>{numFiles}</StatNumber>
            <StatLabel>Number of files</StatLabel>
          </Stat>
        ) : null}
        {numLiveDocs ? (
          <Stat>
            <StatNumber>{numLiveDocs}</StatNumber>
            <StatLabel>Number of live docs</StatLabel>
          </Stat>
        ) : null}
        {numPrimaryDocs ? (
          <Stat>
            <StatNumber>{numPrimaryDocs}</StatNumber>
            <StatLabel>Number of primary docs</StatLabel>
          </Stat>
        ) : null}
        {numSegments ? (
          <Stat>
            <StatNumber>{numSegments}</StatNumber>
            <StatLabel>Number of segments</StatLabel>
          </Stat>
        ) : null}
      </Flex>
    );
  }
  const memoryStat = figures?.memory;
  if (!memoryStat) {
    return null;
  }
  return (
    <Flex>
      <Stat>
        <StatNumber>{window.prettyBytes(memoryStat)}</StatNumber>
        <StatLabel>Memory</StatLabel>
      </Stat>
    </Flex>
  );
};
