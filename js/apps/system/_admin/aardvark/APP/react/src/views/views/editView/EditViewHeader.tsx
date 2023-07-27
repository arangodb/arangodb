import { ArrowBackIcon, CheckIcon, DeleteIcon } from "@chakra-ui/icons";
import { Box, Button, Flex, Grid, IconButton } from "@chakra-ui/react";
import { useField, useFormikContext } from "formik";
import React from "react";
import { useHistory } from "react-router-dom";
import { useEditViewContext } from "../editView/EditViewContext";
import { ViewPropertiesType } from "../View.types";
import { CopyPropertiesDropdown } from "./CopyPropertiesDropdown";
import { DeleteViewModal } from "./DeleteViewModal";
import { EditableViewNameField } from "./EditableViewNameField";

export const EditViewHeader = () => {
  const history = useHistory();
  const { isAdminUser } = useEditViewContext();
  return (
    <Box padding="4" borderBottomWidth="2px" borderColor="gray.200">
      <Flex direction="row" alignItems="center">
        <IconButton
          aria-label="Back"
          variant="ghost"
          size="sm"
          icon={<ArrowBackIcon width="24px" height="24px" />}
          marginRight="2"
          onClick={() => {
            history.push("/views");
          }}
        />
        <EditableViewHeading />
        <DeleteViewButton />
      </Flex>
      {isAdminUser && <ViewActions />}
    </Box>
  );
};

const ViewActions = () => {
  const { errors, changed, isAdminUser } = useEditViewContext();
  const { submitForm } = useFormikContext<ViewPropertiesType>();
  return (
    <Grid gridTemplateColumns="1fr 0.5fr" marginTop="4">
      <CopyPropertiesDropdown />
      <Grid justifyContent={"end"} alignItems={"center"}>
        <Button
          size="xs"
          colorScheme="green"
          leftIcon={<CheckIcon />}
          onClick={submitForm}
          isDisabled={errors.length > 0 || !changed || !isAdminUser}
        >
          Save view
        </Button>
      </Grid>
    </Grid>
  );
};

const EditableViewHeading = () => {
  const { isAdminUser, isCluster } = useEditViewContext();
  const { values } = useFormikContext<ViewPropertiesType>();
  const [, , helpers] = useField("name");
  return (
    <EditableViewNameField
      view={values}
      isAdminUser={isAdminUser}
      isCluster={isCluster}
      setCurrentName={(name: string) => {
        helpers.setValue(name);
      }}
    />
  );
};

const DeleteViewButton = () => {
  const history = useHistory();
  const [showDeleteModal, setShowDeleteModal] = React.useState(false);
  const { initialView } = useEditViewContext();
  return (
    <>
      <Button
        size="xs"
        marginLeft="auto"
        leftIcon={<DeleteIcon />}
        colorScheme="red"
        onClick={() => {
          setShowDeleteModal(true);
        }}
      >
        Delete
      </Button>
      {showDeleteModal && (
        <DeleteViewModal
          view={initialView}
          onSuccess={() => {
            history.push("/views");
          }}
          onClose={() => {
            setShowDeleteModal(false);
          }}
        />
      )}
    </>
  );
};
