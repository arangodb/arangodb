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
import { IndexRowType, StoredValue } from "./useFetchIndices";
import { useSyncIndexCreationJob } from "./useSyncIndexCreationJob";

const TABLE_HEADERS = [
  { id: "id", name: "ID" },
  { id: "type", name: "Type" },
  { id: "unique", name: "Unique" },
  { id: "sparse", name: "Sparse" },
  { id: "extras", name: "Extras" },
  { id: "selectivityEstimate", name: "Selectivity Est." },
  { id: "fields", name: "Fields" },
  { id: "storedValues", name: "Stored Values" },
  { id: "name", name: "Name" },
  { id: "action", name: "Action" }
];

export const CollectionIndicesTable = ({
  indices
}: {
  indices?: IndexRowType[];
}) => {
  useSyncIndexCreationJob();
  return (
    <TableContainer border="1px solid" borderColor="gray.200">
      <Table whiteSpace="normal" size="sm" variant="striped" colorScheme="gray">
        <Thead>
          <Tr height="10">
            {TABLE_HEADERS.map(tableHeader => {
              return <Th key={tableHeader.id}>{tableHeader.name}</Th>;
            })}
          </Tr>
        </Thead>
        <Tbody>
          {indices?.map(indexRow => {
            const indexRowData = getIndexRowData(indexRow);
            return (
              <Tr key={indexRow.id}>
                {TABLE_HEADERS.map(tableHeader => {
                  const tableCellContent =
                    indexRowData[
                      (tableHeader.id as unknown) as keyof typeof indexRowData
                    ];
                  if (tableHeader.id === "action") {
                    return (
                      <Td key={tableHeader.id}>
                        <IndexActionCell indexRow={indexRow} />
                      </Td>
                    );
                  }
                  return <Td key={tableHeader.id}>{tableCellContent}</Td>;
                })}
              </Tr>
            );
          })}
        </Tbody>
      </Table>
    </TableContainer>
  );
};

const getIndexRowData = (indexRow: IndexRowType) => {
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
        return field.name;
      }
      return field;
    })
    .join(", ");
  let storedValuesString = storedValues?.join(", ");
  if (type === "inverted") {
    storedValuesString = (storedValues as StoredValue[])
      ?.map(value => value.fields.join(", "))
      .join(", ");
  }
  return {
    id: indexId,
    extras: extras.join(", "),
    fields: fieldsString,
    sparse: sparse === undefined ? "n/a" : `${sparse}`,
    selectivityEstimate: selectivityEstimateString,
    storedValues: storedValuesString,
    unique,
    name,
    type
  };
};
