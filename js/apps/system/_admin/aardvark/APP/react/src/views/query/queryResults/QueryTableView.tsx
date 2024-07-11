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
    // if there are no valid rows, everything is null or undefined
    return (
      <Text padding="4">
        Could not produce a table, please check the JSON result tab
      </Text>
    );
  }
  const resultHeaders = queryResult.result?.flatMap(resultItem =>
    resultItem ? Object.keys(resultItem) : []
  );
  if (!resultHeaders?.length) {
    return null;
  }
  // dedeupe across all headers
  const headers = Array.from(new Set(resultHeaders));
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
                    /**
                     *  If any row is null, or undefined,
                     *  we display it in the first cell
                     * */
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
                  const isString = typeof value === "string";
                  return (
                    <Td whiteSpace="normal" key={header}>
                      {isString ? value : JSON.stringify(value)}
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
