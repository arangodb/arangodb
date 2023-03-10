import { DeleteIcon, LockIcon } from "@chakra-ui/icons";
import {
  Box,
  Button,
  IconButton,
  Stack,
  Table,
  TableContainer,
  Tbody,
  Td,
  Th,
  Thead,
  Tr,
  useDisclosure
} from "@chakra-ui/react";
import { isNumber } from "lodash";
import React from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { useCollectionIndicesContext } from "./CollectionIndicesContext";
import { IndexType } from "./useFetchIndices";

export const CollectionIndicesList = ({
  indices
}: {
  indices?: IndexType[];
}) => {
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
                extras = [...extras, `${key}: ${(indexRow as any)[key]}`];
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
                  <ActionCell indexRow={indexRow} />
                </Td>
              </Tr>
            );
          })}
        </Tbody>
      </Table>
    </TableContainer>
  );
};
const ActionCell = ({ indexRow }: { indexRow: IndexType }) => {
  const { type } = indexRow;
  if (type === "primary" || type === "edge") {
    return (
      <Box display="flex" justifyContent="center">
        <LockIcon />
      </Box>
    );
  }
  return <DeleteButton indexRow={indexRow} />;
};

const DeleteButton = ({ indexRow }: { indexRow: IndexType }) => {
  const { onOpen, onClose, isOpen } = useDisclosure();
  return (
    <Box display="flex" justifyContent="center">
      <IconButton
        colorScheme="red"
        variant="ghost"
        size="sm"
        aria-label="Delete Index"
        icon={<DeleteIcon />}
        onClick={onOpen}
      />
      <DeleteModal indexRow={indexRow} onClose={onClose} isOpen={isOpen} />
    </Box>
  );
};

const DeleteModal = ({
  onClose,
  isOpen,
  indexRow
}: {
  onClose: () => void;
  isOpen: boolean;
  indexRow: IndexType;
}) => {
  const { onDeleteIndex } = useCollectionIndicesContext();
  return (
    <Modal onClose={onClose} isOpen={isOpen}>
      <ModalHeader>
        Delete Index: "{indexRow.name}" (ID: {indexRow.id})?
      </ModalHeader>
      <ModalBody></ModalBody>
      <ModalFooter>
        <Stack direction="row">
          <Button onClick={onClose}>Cancel</Button>
          <Button
            colorScheme="red"
            onClick={() =>
              onDeleteIndex({ id: indexRow.id, onSuccess: onClose })
            }
          >
            Delete
          </Button>
        </Stack>
      </ModalFooter>
    </Modal>
  );
};
