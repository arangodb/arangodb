import { Button, Flex, Heading, Stack, VStack } from "@chakra-ui/react";
import CryptoJS from "crypto-js";
import { Form, Formik } from "formik";
import _ from "lodash";
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
import { CreateDatabaseUserValues } from "./CreateUser.types";

const addUserFields = {
  user: {
    name: "user",
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
    name: "extra.img",
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
  role: "",
  active: true,
  user: "",
  passwd: "",
  name: "",
  extra: {
    img: "",
    name: ""
  }
};

const parseImgString = (img: string) => {
  // if already md5
  if (img.indexOf("@") === -1) {
    return img;
  }
  // else generate md5
  return CryptoJS.MD5(img).toString();
};

export const AddUserModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const initialFocusRef = React.useRef<HTMLInputElement>(null);
  const handleSubmit = async (values: CreateDatabaseUserValues) => {
    const userOptions = {
      user: values.user,
      active: values.active,
      extra: {
        name: values.name,
        img: values.extra.img
      },
      passwd: values.passwd
    };
    const profileImg = parseImgString(values.extra.img);
    if (!_.isEmpty(profileImg)) {
      userOptions.extra.img = profileImg;
    }

    if (window.frontendConfig.isEnterprise && values.role) {
      userOptions.user = ":role:" + values.user;
      delete userOptions.passwd;
    }

    try {
      const currentDB = getCurrentDB();
      const route = currentDB.route("_api/user");
      const info = await route.post(userOptions);
      window.arangoHelper.arangoNotification(
        "User",
        `Successfully created the user: ${userOptions.user}`
      );
      mutate("/users");
      onClose();
      return info;
    } catch (e: any) {
      const errorMessage = e.response.body.errorMessage;
      window.arangoHelper.arangoError("Could not create user", errorMessage);
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
            Create new user
          </Heading>
        </Flex>
      </ModalHeader>
      <ModalBody>
        <Formik
          initialValues={INITIAL_VALUES}
          validationSchema={Yup.object({
            user: Yup.string().required("Username is required")
          })}
          onSubmit={handleSubmit}
        >
          {({ isSubmitting }) => (
            <Form>
              <VStack spacing={4} align="stretch">
                <FieldsGrid maxWidth="full">
                  <FormField field={addUserFields.user} />
                  <FormField field={addUserFields.name} />
                  <FormField field={addUserFields.gravatar} />
                  <FormField field={addUserFields.passwd} />
                  {window.frontendConfig.isEnterprise && (
                    <FormField field={addUserFields.role} />
                  )}
                  <FormField field={addUserFields.active} />
                </FieldsGrid>
                <ModalFooter>
                  <Stack direction="row" spacing={4} align="center">
                    <Button onClick={onClose} colorScheme="gray">
                      Cancel
                    </Button>

                    <Button
                      colorScheme="blue"
                      type="submit"
                      isLoading={isSubmitting}
                    >
                      Create
                    </Button>
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
