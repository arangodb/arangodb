import {
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
            var {
              indexId,
              extras,
              fields,
              selectivityEstimate,
              storedValues,
              sparse,
              unique,
              name,
              type
            } = getIndexRowData(indexRow);
            return (
              <Tr>
                <Td>{indexId}</Td>
                <Td>{type}</Td>
                <Td>{unique}</Td>
                <Td>{sparse}</Td>
                <Td>{extras}</Td>
                <Td>{selectivityEstimate}</Td>
                <Td>{fields}</Td>
                <Td>{storedValues}</Td>
                <Td>{name}</Td>
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

const getIndexRowData = (indexRow: IndexType) => {
  const {
    fields,
    storedValues,
    id,
    selectivityEstimate,
    sparse,
    unique,
    name,
    type
  } = indexRow;
  const position = id.indexOf("/");
  const indexId = id.substring(position + 1, id.length);
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
  const selectivityEstimateString = isNumber(selectivityEstimate)
    ? `${(selectivityEstimate * 100).toFixed(2)}%`
    : "n/a";
  const fieldsString = fields
    .map(field => {
      if (typeof field === "object") {
        return (field as any).name;
      }
      return field;
    })
    .join(", ");
  return {
    indexId,
    extras: extras.join(", "),
    fields: fieldsString,
    sparse: sparse === undefined ? "n/a" : `${sparse}`,
    selectivityEstimate: selectivityEstimateString,
    storedValues: storedValues?.join(", "),
    unique,
    name,
    type
  };
};
