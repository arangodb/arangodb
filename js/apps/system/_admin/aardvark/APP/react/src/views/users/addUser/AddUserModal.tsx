import { Button, Flex, Heading, Stack, VStack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { mutate } from "swr";
import * as Yup from "yup";
import { FormField } from "../../../components/form/FormField";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { getCurrentDB } from "../../../utils/arangoClient";
import { FieldsGrid } from "../FieldsGrid";
import { useUsersModeContext } from "../UsersModeContext";
import { CreateDatabaseUserValues } from "./CreateUser.types";

const addUserFields = {
  username: {
    name: "username",
    type: "text",
    label: "Username",
    isRequired: true
  },
  name: {
    name: "name",
    type: "text",
    label: "Name",
    isRequired: false
  },
  gravatar: {
    name: "gravatar",
    type: "text",
    label: "Gravatar account (Mail)",
    tooltip:
      "Mailaddress or its md5 representation of your gravatar account.The address will be converted into a md5 string. Only the md5 string will be stored, not the mailaddress.",
    isRequired: false
  },
  passwd: {
    name: "passwd",
    type: "password", // does the type "password" exist in FormField?
    label: "Password",
    isRequired: false
  },
  role: {
    name: "role",
    type: "boolean",
    label: "Role",
    isRequired: false
  },
  active: {
    name: "active",
    type: "boolean",
    label: "Active",
    isRequired: false
  }
};

const INITIAL_VALUES: CreateDatabaseUserValues = {
  name: "",
  active: true,
  username: "",
  gravatar: "",
  passwd: ""
};

export const AddUserModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const initialFocusRef = React.useRef<HTMLInputElement>(null);
  const { initialUser, mode } = useUsersModeContext();
  console.log("initialUser: ", initialUser);
  const handleSubmit = async (values: CreateDatabaseUserValues) => {
    const currentDB = getCurrentDB();

    const graph = currentDB.graph(values.name);
    console.log("graph: ", graph);

    try {
      // Here goes the API call to create the new user
      /*
      const info = await graph.create(values.edgeDefinitions, {
        orphanCollections: values.orphanCollections
      });
      */
      const info = "API feedback";
      window.arangoHelper.arangoNotification(
        "User",
        `Successfully created the user: ${values.username}`
      );
      mutate("/users");
      onClose();
      return info;
    } catch (e: any) {
      const errorMessage = e.response.body.errorMessage;
      window.arangoHelper.arangoError("Could not add graph", errorMessage);
    }
  };
  return (
    <Modal
      initialFocusRef={initialFocusRef}
      size="6xl"
      isOpen={isOpen}
      onClose={onClose}
    >
      <ModalHeader fontSize="sm" fontWeight="normal">
        <Flex direction="row" alignItems="center">
          <Heading marginRight="4" size="md">
            {mode === "add" ? "Create new user" : "Edit user"}
          </Heading>
        </Flex>
      </ModalHeader>
      <ModalBody>
        <Formik
          //initialValues={initialUser || INITIAL_VALUES}
          initialValues={INITIAL_VALUES}
          validationSchema={Yup.object({
            username: Yup.string().required("Username is required")
          })}
          onSubmit={handleSubmit}
        >
          {({ isSubmitting }) => (
            <Form>
              <VStack spacing={4} align="stretch">
                <FieldsGrid maxWidth="full">
                  <FormField
                    field={{
                      ...addUserFields.username
                    }}
                  />
                  <FormField
                    field={{
                      ...addUserFields.name
                    }}
                  />
                  <FormField
                    field={{
                      ...addUserFields.gravatar
                    }}
                  />
                  <FormField
                    field={{
                      ...addUserFields.passwd
                    }}
                  />
                  <FormField
                    field={{
                      ...addUserFields.role
                    }}
                  />
                  <FormField
                    field={{
                      ...addUserFields.active
                    }}
                  />
                </FieldsGrid>
                <ModalFooter>
                  <Stack direction="row" spacing={4} align="center">
                    <Button onClick={onClose} colorScheme="gray">
                      Cancel
                    </Button>
                    {mode === "add" && (
                      <Button
                        colorScheme="blue"
                        type="submit"
                        isLoading={isSubmitting}
                      >
                        Create
                      </Button>
                    )}
                  </Stack>
                </ModalFooter>
              </VStack>
            </Form>
          )}
        </Formik>
      </ModalBody>
    </Modal>
  );
};
