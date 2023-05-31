import { DeleteIcon } from "@chakra-ui/icons";
import { Button } from "@chakra-ui/react";
import React, { useState } from "react";
import { getApiRouteForCurrentDB } from "../../utils/arangoClient";
import { encodeHelper } from "../../utils/encodeHelper";
import { FormState } from "./constants";
import { DeleteViewModal } from "./DeleteViewModal";

declare var arangoHelper: { [key: string]: any };
declare var window: { [key: string]: any };

type DeleteButtonWrapProps = {
  view: FormState;
  disabled?: boolean;
};

export const DeleteButtonWrap = ({ view, disabled }: DeleteButtonWrapProps) => {
  const [show, setShow] = useState(false);

  const handleDelete = async () => {
    try {
      const { encoded: encodedViewName } = encodeHelper(view.name);
      const result = await getApiRouteForCurrentDB().delete(
        `/view/${encodedViewName}`
      );

      if (result.body.error) {
        arangoHelper.arangoError(
          "Failure",
          `Got unexpected server response: ${result.body.errorMessage}`
        );
      } else {
        window.App.navigate("#views", { trigger: true });
        arangoHelper.arangoNotification(
          "Success",
          `Deleted View: ${view.name}`
        );
      }
    } catch (e: any) {
      arangoHelper.arangoError(
        "Failure",
        `Got unexpected server response: ${e.message}`
      );
    }
  };

  return (
    <>
      <Button
        size="xs"
        colorScheme="red"
        leftIcon={<DeleteIcon />}
        onClick={() => setShow(true)}
        isDisabled={disabled}
      >
        Delete
      </Button>
      <DeleteViewModal
        isOpen={show}
        onClose={() => setShow(false)}
        view={view}
        onDelete={handleDelete}
      />
    </>
  );
};
