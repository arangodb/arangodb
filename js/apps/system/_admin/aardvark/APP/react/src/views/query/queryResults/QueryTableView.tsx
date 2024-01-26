import {
  Table,
  TableContainer,
  Tbody,
  Td,
  Text,
  Th,
  Thead,
  Tr
} from "@chakra-ui/react";
import React from "react";
import { QueryResultType } from "../ArangoQuery.types";

export const QueryTableView = ({
  queryResult
}: {
  queryResult: QueryResultType<({ [key: string]: any } | null)[]>;
}) => {
  const firstValidResult = queryResult.result?.find(
    result => result !== null && result !== undefined
  );
  if (!firstValidResult) {
    return (
      <>
        <Text>Could not produce a table, please check the JSON result tab</Text>
      </>
    );
  }
  const headers = Object.keys(firstValidResult);
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
                {headers.map((header, headerIndex) => {
                  if (row === null || row === undefined) {
                    if (headerIndex === 0) {
                      return (
                        <Td colSpan={headers.length} key={header}>
                          {JSON.stringify(row)}
                        </Td>
                      );
                    }
                    return null;
                  }
                  const value = row[header];
                  if (typeof value === "string") {
                    return <Td key={header}>{value}</Td>;
                  }
                  return (
                    <Td whiteSpace="normal" key={header}>
                      {JSON.stringify(value)}
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
