import { AddIcon } from "@chakra-ui/icons";
import { Button, Heading, Stack, Tooltip } from "@chakra-ui/react";
import React from "react";
import { useDatabaseReadOnly } from "../../utils/useDatabaseReadOnly";

export const ListHeader = ({
  onButtonClick,
  heading,
  buttonText
}: {
  onButtonClick: () => void;
  heading: string;
  buttonText: string;
}) => {
  const { readOnly, isLoading } = useDatabaseReadOnly();
  return (
    <Stack direction="row" marginBottom="4" alignItems="center">
      <Heading size="lg">{heading}</Heading>
      <Tooltip
        isDisabled={!(readOnly || isLoading)}
        hasArrow
        label={
          readOnly
            ? "You have read-only permissions to this resource"
            : isLoading
            ? "Loading..."
            : ""
        }
        shouldWrapChildren
      >
        <Button
          size="sm"
          isLoading={isLoading}
          isDisabled={readOnly}
          leftIcon={<AddIcon />}
          colorScheme="green"
          onClick={onButtonClick}
        >
          {buttonText}
        </Button>
      </Tooltip>
    </Stack>
  );
};
