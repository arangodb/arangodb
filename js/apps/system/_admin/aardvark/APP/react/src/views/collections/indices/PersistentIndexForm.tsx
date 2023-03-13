import { Box, Button, FormLabel, Stack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import * as Yup from "yup";
import { InputControl } from "../../../components/form/InputControl";
import { SwitchControl } from "../../../components/form/SwitchControl";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { useCollectionIndicesContext } from "./CollectionIndicesContext";

const handleError = (error: { errorMessage: string }) => {
  if (error.errorMessage) {
    window.arangoHelper.arangoError("Index error", error.errorMessage);
  } else {
    window.arangoHelper.arangoError("Index error", "Could not create index.");
  }
};

const handleSuccess = (onSuccess: () => void) => {
  window.arangoHelper.arangoNotification(
    "Index",
    "Creation in progress. This may take a while."
  );
  onSuccess();
};
const postCreateIndex = async ({
  collectionName,
  values,
  onSuccess
}: {
  collectionName: string;
  values: typeof initialValues;
  onSuccess: () => void;
}) => {
  window.arangoHelper.checkDatabasePermissions(
    function() {
      window.arangoHelper.arangoError(
        "You do not have permission to create indexes in this database."
      );
    },
    async () => {
      let result;
      try {
        result = await getApiRouteForCurrentDB().post(
          `index`,
          {
            type: values.type,
            fields: values.fields.split(","),
            inBackground: values.inBackground,
            name: values.name
          },
          `collection=${collectionName}`
        );
        if (result.body.code === 201) {
          handleSuccess(onSuccess);
        }
      } catch (error) {
        handleError(error.response.body);
      }
      // $.ajax({
      //   cache: false,
      //   type: "POST",
      //   url: window.arangoHelper.databaseUrl(
      //     "/_api/index?collection=" + encodeURIComponent(self.get("name"))
      //   ),
      //   headers: {
      //     "x-arango-async": "store"
      //   },
      //   data: JSON.stringify(postParameter),
      //   contentType: "application/json",
      //   processData: false,
      //   success: function(data, textStatus, xhr) {
      //     if (xhr.getResponseHeader("x-arango-async-id")) {
      //       window.arangoHelper.addAardvarkJob({
      //         id: xhr.getResponseHeader("x-arango-async-id"),
      //         type: "index",
      //         desc: "Creating Index",
      //         collection: self.get("id")
      //       });
      //       callback(false, data);
      //     } else {
      //       callback(true, data);
      //     }
      //   },
      //   error: function(data) {
      //     callback(true, data);
      //   }
      // });
    }
  );
};

const initialValues = {
  type: "persistent",
  fields: "",
  name: "",
  storedValues: "",
  unique: false,
  sparse: false,
  deduplicate: false,
  estimates: true,
  cacheEnabled: false,
  inBackground: true
};

export const PersistentIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { collectionName, onCloseForm } = useCollectionIndicesContext();
  return (
    <Formik
      onSubmit={async values => {
        await postCreateIndex({
          collectionName,
          values,
          onSuccess: onCloseForm
        });
      }}
      initialValues={initialValues}
      validationSchema={Yup.object({
        fields: Yup.string().required("Fields are required")
      })}
    >
      {({ isSubmitting, isValid }) => (
        <Form>
          <Stack spacing="4">
            <Box display={"grid"} gridTemplateColumns={"200px 1fr"} rowGap="5">
              <FormLabel htmlFor="fields">Fields</FormLabel>
              <InputControl isRequired name="fields" />
              <FormLabel htmlFor="name">Name</FormLabel>
              <InputControl name="name" />
              <FormLabel htmlFor="minLength">Min. Length</FormLabel>
              <InputControl
                inputProps={{
                  type: "number"
                }}
                name="minLength"
              />
              <FormLabel htmlFor="inBackground">Create in background</FormLabel>
              <SwitchControl name="inBackground" />
            </Box>
            <Stack direction="row" justifyContent="flex-end">
              <Button size="sm" onClick={onClose}>
                Close
              </Button>
              <Button
                colorScheme="green"
                size="sm"
                type="submit"
                isDisabled={!isValid}
                isLoading={isSubmitting}
              >
                Create
              </Button>
            </Stack>
          </Stack>
        </Form>
      )}
    </Formik>
  );
};
