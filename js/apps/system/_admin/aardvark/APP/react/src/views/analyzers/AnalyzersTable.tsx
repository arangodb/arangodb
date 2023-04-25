import {
  Table,
  TableContainer,
  Tbody,
  Td,
  Th,
  Thead,
  Tr
} from "@chakra-ui/react";
import React from "react";
import { useAnalyzersContext } from "./AnalyzersContext";

const TABLE_HEADERS = [
  { id: "db", name: "DB" },
  { id: "name", name: "Name" },
  { id: "type", name: "Type" },
  { id: "actions", name: "Actions" }
];

export const AnalyzersTable = () => {
  const { analyzers } = useAnalyzersContext();
  return (
    <TableContainer
      border="1px solid"
      borderColor="gray.200"
      backgroundColor="white"
    >
      <Table whiteSpace="normal" size="sm" variant="striped" colorScheme="gray">
        <Thead>
          <Tr height="10">
            {TABLE_HEADERS.map(tableHeader => {
              return <Th key={tableHeader.id}>{tableHeader.name}</Th>;
            })}
          </Tr>
        </Thead>
        <Tbody>
          {analyzers?.map(analyzerRow => {
            return (
              <Tr key={analyzerRow.name}>
                {TABLE_HEADERS.map(tableHeader => {
                  const tableCellContent =
                    analyzerRow[
                      tableHeader.id as unknown as keyof typeof analyzerRow
                    ];
                  if (tableHeader.id === "actions") {
                    return <Td key={tableHeader.id}>view delete</Td>;
                  }
                  if (tableHeader.id === "db") {
                    return (
                      <Td key={tableHeader.id}>
                        {analyzerRow.name.includes("::")
                          ? analyzerRow.name.split("::")[0]
                          : "_system"}
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
