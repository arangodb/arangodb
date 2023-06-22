import {
  Box,
  Flex,
  Input,
  Table,
  TableContainer,
  Tag,
  Tbody,
  Td,
  Text,
  Th,
  Thead,
  Tr
} from "@chakra-ui/react";
import { ValidationError } from "jsoneditor-react";
import React from "react";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { JSONErrors } from "../../../components/jsonEditor/JSONErrors";
import { useQueryContext } from "../QueryContextProvider";

const detectType = (value: string) => {
  try {
    const parsedValue = JSON.parse(value);
    if (parsedValue instanceof Array) {
      return "array";
    }
    return typeof parsedValue;
  } catch (ignore) {}
};
const parseInput = (value: string) => {
  try {
    return JSON.parse(value);
  } catch (e) {
    return value;
  }
};
const definedType = ["string", "number", "array", "object", "boolean"];
export const BindVariablesTab = ({ mode }: { mode: "json" | "table" }) => {
  const { queryBindParams, onBindParamsChange } = useQueryContext();
  const [errors, setErrors] = React.useState<ValidationError[]>([]);
  if (mode === "table") {
    return (
      <TableContainer>
        <Table size="sm">
          <Thead>
            <Th>Key</Th>
            <Th>Value</Th>
          </Thead>
          <Tbody>
            {Object.keys(queryBindParams).map(key => {
              return <BindVariableRow key={key} bindParamKey={key} />;
            })}
          </Tbody>
          {Object.keys(queryBindParams).length === 0 ? (
            <Tr>
              <Td colSpan={2}>No bind parameters found in query</Td>
            </Tr>
          ) : null}
        </Table>
      </TableContainer>
    );
  }
  return (
    <Box position="relative" height="full">
      <ControlledJSONEditor
        mode="code"
        value={queryBindParams}
        onChangeJSON={updatedValue => {
          onBindParamsChange(updatedValue);
        }}
        htmlElementProps={{
          style: {
            height: "calc(100% - 20px)"
          }
        }}
        onValidationError={errors => {
          setErrors(errors);
        }}
        // @ts-ignore
        mainMenuBar={false}
      />
      <JSONErrors
        position="absolute"
        bottom={0}
        left={0}
        right={0}
        zIndex={10}
        errors={errors}
      />
    </Box>
  );
};

const BindVariableRow = ({ bindParamKey }: { bindParamKey: string }) => {
  const [type, setType] = React.useState<
    "string" | "number" | "array" | "object" | "boolean" | "unknown"
  >("unknown");
  const { queryBindParams, onBindParamsChange } = useQueryContext();
  const value = queryBindParams[bindParamKey];
  let parsedValue = value;
  if (typeof value === "object") {
    parsedValue = JSON.stringify(value);
  }
  return (
    <Tr>
      <Td>
        <Flex width="200px" alignItems="center" gap="2">
          <Text isTruncated>{bindParamKey}</Text>
          {definedType.includes(type) && (
            <Tag flexShrink={0} size="sm">
              {type}
            </Tag>
          )}
        </Flex>
      </Td>
      <Td>
        <Input
          size="sm"
          placeholder="Value"
          value={parsedValue}
          onChange={e => {
            setType(detectType(e.target.value) as any);
            const parsedValue = parseInput(e.target.value);
            const newQueryBindParams = {
              ...queryBindParams,
              [bindParamKey]: parsedValue
            };
            onBindParamsChange(newQueryBindParams);
          }}
        />
      </Td>
    </Tr>
  );
};
