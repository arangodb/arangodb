import {
  Table,
  TableContainer,
  Tbody,
  Td,
  Th,
  Thead,
  Tr
} from "@chakra-ui/react";
import { isNumber, round } from "lodash";
import React from "react";
import { IndexActionCell } from "./IndexActionCell";
import { IndexType } from "./useFetchIndices";
import { useSyncIndexCreationJob } from "./useSyncIndexCreationJob";

export const CollectionIndicesTable = ({
  indices
}: {
  indices?: IndexType[];
}) => {
  useSyncIndexCreationJob();
  return (
    <TableContainer border="1px solid" borderColor="gray.200">
      <Table whiteSpace="normal" size="sm" variant="striped" colorScheme="gray">
        <Thead>
          <Tr height="10">
            <Th>ID</Th>
            <Th>Type</Th>
            <Th>Unique</Th>
            <Th>Sparse</Th>
            <Th>Extras</Th>
            <Th>Selectivity Est.</Th>
            <Th>Fields</Th>
            <Th>Stored Values</Th>
            <Th>Name</Th>
            <Th>Action</Th>
          </Tr>
        </Thead>
        <Tbody>
          {indices?.map(indexRow => {
            var { indexId, extras, selectivityEstimate } = getIndexRowData(
              indexRow
            );
            return (
              <Tr>
                <Td>{indexId}</Td>
                <Td>{indexRow.type}</Td>
                <Td>{`${indexRow.unique}`}</Td>
                <Td>{`${indexRow.sparse}`}</Td>
                <Td>{JSON.stringify(extras)}</Td>
                <Td>{selectivityEstimate}</Td>
                <Td>{JSON.stringify(indexRow.storedValues)}</Td>
                <Td>{JSON.stringify(indexRow.fields)}</Td>
                <Td>{indexRow.name}</Td>
                <Td>
                  <IndexActionCell indexRow={indexRow} />
                </Td>
              </Tr>
            );
          })}
        </Tbody>
      </Table>
    </TableContainer>
  );
};

function getIndexRowData(indexRow: IndexType) {
  const position = indexRow.id.indexOf("/");
  const indexId = indexRow.id.substring(position + 1, indexRow.id.length);

  let extras: string[] = [];
  [
    "deduplicate",
    "expireAfter",
    "minLength",
    "geoJson",
    "estimates",
    "cacheEnabled"
  ].forEach(function(key) {
    if (indexRow.hasOwnProperty(key)) {
      extras = [...extras, `${key}: ${(indexRow as any)[key]}`];
    }
  });
  const selectivityEstimate = isNumber(indexRow.selectivityEstimate)
    ? `${round(indexRow.selectivityEstimate * 100, 2)}%`
    : "N/A";
  return { indexId, extras, selectivityEstimate };
}
