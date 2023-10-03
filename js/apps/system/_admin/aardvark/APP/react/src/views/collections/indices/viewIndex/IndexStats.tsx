import React from "react";
import { Flex, Stat, StatLabel, StatNumber } from "@chakra-ui/react";
import { IndexWithFigures } from "../useFetchCollectionIndices";

export const IndexStats = ({
  currentIndex
}: {
  currentIndex: IndexWithFigures;
}) => {
  const { figures, type } = currentIndex;

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
        {indexSize && (
          <Stat>
            <StatNumber>{window.prettyBytes(indexSize)}</StatNumber>
            <StatLabel>Index size</StatLabel>
          </Stat>
        )}
        {numDocs && (
          <Stat>
            <StatNumber>{numDocs}</StatNumber>
            <StatLabel>Number of docs</StatLabel>
          </Stat>
        )}
        {numFiles && (
          <Stat>
            <StatNumber>{numFiles}</StatNumber>
            <StatLabel>Number of files</StatLabel>
          </Stat>
        )}
        {numLiveDocs && (
          <Stat>
            <StatNumber>{numLiveDocs}</StatNumber>
            <StatLabel>Number of live docs</StatLabel>
          </Stat>
        )}
        {numPrimaryDocs && (
          <Stat>
            <StatNumber>{numPrimaryDocs}</StatNumber>
            <StatLabel>Number of primary docs</StatLabel>
          </Stat>
        )}
        {numSegments && (
          <Stat>
            <StatNumber>{numSegments}</StatNumber>
            <StatLabel>Number of segments</StatLabel>
          </Stat>
        )}
      </Flex>
    );
  }
  const memoryStat = figures?.memory;
  return (
    <Flex>
      <Stat>
        <StatNumber>{window.prettyBytes(memoryStat || 0)}</StatNumber>
        <StatLabel>Memory</StatLabel>
      </Stat>
    </Flex>
  );
};
