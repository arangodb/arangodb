import { Box } from "@chakra-ui/react";
import React from "react";
import { ListHeader } from "../../components/table/ListHeader";
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
  return (
    <ListHeader
      onButtonClick={() => {
        window.App.navigate("#services/install", { trigger: true });
      }}
      heading="Services"
      buttonText="Add service"
    />
  );
};
