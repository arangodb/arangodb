import { InfoIcon } from "@chakra-ui/icons";
import {
  Flex,
  Popover,
  PopoverArrow,
  PopoverBody,
  PopoverContent,
  PopoverTrigger, Text
} from "@chakra-ui/react";
import React from "react";
import { ExternalLink } from "../../../components/link/ExternalLink";

export const CollectionDefaultRowWarningPopover = ({
  databaseName
}: {
  databaseName: string;
}) => {
  return (
    <Popover trigger="hover" placement="right">
      <PopoverTrigger>
        <InfoIcon />
      </PopoverTrigger>
      <PopoverContent
        backgroundColor="gray.900"
        color="white"
        _focus={{
          outline: "none"
        }}
        fontWeight="500"
      >
        <PopoverArrow backgroundColor="gray.900" />
        <PopoverBody>
          <Flex direction="column" gap="2">
            <Text>
              Default access level for collections in "{databaseName}", if
              authentication level is not specified.
            </Text>

            <Text as="span">
              <Text as="span" color="orange.300">
                Warning
              </Text>
              : This cannot be used to restrict access configured on the default
              database level.
              <ExternalLink
                display="inline"
                color="blue.300"
                _hover={{
                  color: "blue.400"
                }}
                href="https://docs.arangodb.com/stable/operations/administration/user-management/#wildcard-collection-access-level"
              >
                Learn more
              </ExternalLink>
            </Text>
          </Flex>
        </PopoverBody>
      </PopoverContent>
    </Popover>
  );
};
