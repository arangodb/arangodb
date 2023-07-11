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
import { QueryResultType } from "../QueryContextProvider";

export const QueryTableView = ({
  queryResult
}: {
  queryResult: QueryResultType<{ [key: string]: any }[]>;
}) => {
  const firstRow = queryResult.result?.[0];
  const headers = firstRow ? Object.keys(firstRow) : [];
  if (!headers.length) {
    return null;
  }
  return (
    <TableContainer maxHeight="600px" overflowX="auto" overflowY="auto">
      <Table>
        <Thead>
          <Tr>
            {headers.map(header => (
              <Th textTransform="unset" key={header}>
                {header}
              </Th>
            ))}
          </Tr>
        </Thead>
        <Tbody>
          {queryResult.result?.map((row, index) => {
            return (
              <Tr key={index}>
                {headers.map(header => {
                  const value = row[header];
                  if (typeof value === "string") {
                    return <Td key={header}>{value}</Td>;
                  }
                  return (
                    <Td whiteSpace="normal" key={header}>
                      {JSON.stringify(row[header])}
                    </Td>
                  );
                })}
              </Tr>
            );
          })}
        </Tbody>
      </Table>
    </TableContainer>
  );
};
