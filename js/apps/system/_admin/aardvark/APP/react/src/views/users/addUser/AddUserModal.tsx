import { Button, Flex, Heading, Stack, VStack } from "@chakra-ui/react";
import CryptoJS from "crypto-js";
import { Form, Formik } from "formik";
import React from "react";
import { mutate } from "swr";
import * as Yup from "yup";
import { FieldsGrid } from "../../../components/form/FieldsGrid";
import { FormField } from "../../../components/form/FormField";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { getCurrentDB } from "../../../utils/arangoClient";
import { notifyError, notifySuccess } from "../../../utils/notifications";
import { CreateUserValues } from "./CreateUser.types";

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
    type: "password",
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

const INITIAL_VALUES: CreateUserValues = {
  role: false,
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
  // if it's not an email, it's already an md5
  if (img.indexOf("@") === -1) {
    return img;
  }
  // else convert email to md5
  return CryptoJS.MD5(img).toString();
};

const putUser = async ({
  userOptions,
  onSuccess
}: {
  userOptions: {
    user: string;
    active: boolean;
    extra: {
      name?: string;
      img?: string;
    };
    passwd?: string;
  };
  onSuccess: () => void;
}) => {
  try {
    const currentDB = getCurrentDB();
    const route = currentDB.route("_api/user");
    const info = await route.post(userOptions);
    notifySuccess(`Successfully created the user: ${userOptions.user}`);
    mutate("/users");
    onSuccess();
    return info;
  } catch (e: any) {
    const errorMessage = e?.response?.parsedBody?.errorMessage || e.message;
    const finalMessage = errorMessage
      ? `Could not create user: ${errorMessage}`
      : `Could not create user`;
    notifyError(finalMessage);
  }
};

export const AddUserModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const handleSubmit = async (values: CreateUserValues) => {
    const profileImg = values.extra.img && parseImgString(values.extra.img);
    const isRole = window.frontendConfig.isEnterprise && values.role;
    let { user } = values;
    if (isRole) {
      user = `:role:${values.user}`;
    }
    const userOptions = {
      user,
      active: values.active,
      extra: {
        name: values.name,
        img: profileImg || undefined
      },
      passwd: !isRole ? values.passwd : undefined
    };
    await putUser({ userOptions, onSuccess: onClose });
  };
  return (
    <Modal size="6xl" isOpen={isOpen} onClose={onClose}>
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
          {({ isSubmitting, values }) => (
            <Form>
              <VStack spacing={4} align="stretch">
                <FieldsGrid maxWidth="full">
                  <FormField field={addUserFields.user} />
                  <FormField field={addUserFields.name} />
                  <FormField field={addUserFields.gravatar} />
                  <FormField
                    field={{ ...addUserFields.passwd, isDisabled: values.role }}
                  />
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
                      colorScheme="green"
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
