import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Heading, Stack } from "@chakra-ui/react";
import React from "react";
import { useDatabaseReadOnly } from "../../utils/useDatabaseReadOnly";
import { ServicesTable } from "./listServices/ServicesTable";

export const ServicesView = () => {
  return (
    <Box padding="4" width="100%">
      <ServiceViewHeader />
      <ServicesTable />
    </Box>
  );
};

const ServiceViewHeader = () => {
  const { readOnly, isLoading } = useDatabaseReadOnly();
  return (
    <Stack direction="row" marginBottom="4" alignItems="center">
      <Heading size="lg">Services</Heading>
      <Button
        size="sm"
        isLoading={isLoading}
        isDisabled={readOnly}
        leftIcon={<AddIcon />}
        colorScheme="green"
        onClick={() => {
          window.App.navigate("#services/install", { trigger: true });
        }}
      >
        Add service
      </Button>
    </Stack>
  );
};
