import {
  Button,
  Checkbox,
  Flex,
  ListItem,
  Stack,
  Text,
  UnorderedList
} from "@chakra-ui/react";
import React from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { downloadPost } from "../../../utils/downloadHelper";
import { useQueryContext } from "../QueryContextProvider";

export const DebugPackageModal = ({
  isDebugPackageModalOpen,
  onCloseDebugPackageModal
}: {
  isDebugPackageModalOpen: boolean;
  onCloseDebugPackageModal: () => void;
}) => {
  const { queryValue, queryBindParams } = useQueryContext();
  const [includeExamples, setIncludeExamples] = React.useState(true);
  return (
    <Modal
      size="xl"
      isOpen={isDebugPackageModalOpen}
      onClose={onCloseDebugPackageModal}
    >
      <ModalHeader>Download query debug package</ModalHeader>
      <ModalBody>
        <Flex direction="column">
          <Text fontWeight="medium" fontSize="lg">
            Disclaimer
          </Text>
          <Flex direction="column" gap="2">
            <Text>
              This will generate a package containing a lot of commonly required
              information about your query and environment that helps the
              ArangoDB Team to reproduce your issue. This debug package will
              include:
            </Text>
            <UnorderedList marginLeft="4">
              <ListItem>collection names</ListItem>
              <ListItem>collection indexes</ListItem>
              <ListItem>attribute names</ListItem>
              <ListItem>bind parameters</ListItem>
            </UnorderedList>
            <Text>
              Additionally, samples of your data will be included with all
              <Text as="span" fontWeight="medium">
                {" "}
                string values obfuscated in a non-reversible way
              </Text>{" "}
              if below checkbox is ticked.
            </Text>
            <Text>If disabled, this package will not include any data.</Text>
            <Text>
              Please open the package locally and check if it contains anything
              that you are not allowed/willing to share and obfuscate it before
              uploading. Including this package in bug reports will lower the
              amount of questioning back and forth to reproduce the issue on our
              side and is much appreciated.
            </Text>
            <Checkbox
              isChecked={includeExamples}
              onChange={e => {
                setIncludeExamples(e.target.checked);
              }}
            >
              <Text as="span" fontWeight="medium">
                Include obfuscated examples
              </Text>
            </Checkbox>
          </Flex>
        </Flex>
      </ModalBody>
      <ModalFooter>
        <Stack direction="row">
          <Button colorScheme="gray" onClick={() => onCloseDebugPackageModal()}>
            Cancel
          </Button>
          <Button
            colorScheme="green"
            onClick={async () => {
              downloadPost({
                url: "query/debugDump",
                body: {
                  query: queryValue,
                  bindVars: queryBindParams || {},
                  examples: includeExamples
                }
              });
            }}
          >
            Create
          </Button>
        </Stack>
      </ModalFooter>
    </Modal>
  );
};
