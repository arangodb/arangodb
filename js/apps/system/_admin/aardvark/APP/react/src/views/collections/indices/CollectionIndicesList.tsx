import { DeleteIcon } from "@chakra-ui/icons";
import {
  IconButton,
  Table,
  TableContainer,
  Tbody,
  Td,
  Th,
  Thead,
  Tr
} from "@chakra-ui/react";
import { isNumber } from "lodash";
import React from "react";
import { IndexType } from "./useFetchIndices";

export const CollectionIndicesList = ({
  indices
}: {
  indices?: IndexType[];
}) => {
  console.log({ indices });
  return (
    <TableContainer>
      <Table size="sm" variant="striped" colorScheme="gray">
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
            const position = indexRow.id.indexOf("/");
            const indexId = indexRow.id.substring(
              position + 1,
              indexRow.id.length
            );

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
                extras = [
                  ...extras,
                  `${key}: ${(indexRow as any)[key]}`
                ];
              }
            });
            const selectivityEstimate = isNumber(indexRow.selectivityEstimate)
              ? `${indexRow.selectivityEstimate * 100}%`
              : "N/A";

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
                  <IconButton
                    colorScheme="red"
                    variant="ghost"
                    size="sm"
                    aria-label="Delete Index"
                    icon={<DeleteIcon />}
                  />
                </Td>
              </Tr>
            );
          })}
        </Tbody>
      </Table>
    </TableContainer>
  );
};
