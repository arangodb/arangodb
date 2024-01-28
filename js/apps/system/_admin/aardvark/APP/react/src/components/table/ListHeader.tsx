import { AddIcon } from "@chakra-ui/icons";
import {
  Button,
  Heading,
  Stack,
  Tooltip,
  useForceUpdate
} from "@chakra-ui/react";
import React from "react";

export const ListHeader = ({
  onButtonClick,
  heading,
  buttonText
}: {
  onButtonClick: () => void;
  heading: string;
  buttonText: string;
}) => {
  const readOnly = window.App.userCollection.authOptions.ro;
  // HACK: force update for window variable, todo: fix this
  const forceUpdate = useForceUpdate();
  React.useEffect(() => {
    const timeout = window.setTimeout(() => {
      forceUpdate();
    }, 1000);
    // eslint-disable-next-line react-hooks/exhaustive-deps
    return () => window.clearTimeout(timeout);
  }, [forceUpdate]);
  return (
    <Stack direction="row" marginBottom="4" alignItems="center">
      <Heading size="lg">{heading}</Heading>
      <Tooltip
        isDisabled={!readOnly}
        hasArrow
        label={"You have read-only permissions to this resource"}
        shouldWrapChildren
      >
        <Button
          size="sm"
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
