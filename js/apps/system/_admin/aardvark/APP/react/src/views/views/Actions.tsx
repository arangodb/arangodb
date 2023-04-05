import { CheckIcon, DeleteIcon } from "@chakra-ui/icons";
import { Button } from "@chakra-ui/react";
import { pick } from "lodash";
import React, { useState } from "react";
import { mutate } from "swr";
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

type SaveButtonProps = {
  view: FormState;
  disabled?: boolean;
  oldName?: string;
  setChanged: (changed: boolean) => void;
};

export const SaveButtonWrap = ({
  disabled,
  view,
  oldName,
  setChanged
}: SaveButtonProps) => {
  const handleSave = async () => {
    const route = getApiRouteForCurrentDB();
    let result;
    let error = false;
    const { normalized: normalizedViewName, encoded: encodedViewName } =
      encodeHelper(view.name);
    const path = `/view/${encodedViewName}/properties`;

    try {
      if (oldName && view.name !== oldName) {
        const { encoded: encodedOldName } = encodeHelper(oldName);
        result = await route.put(`/view/${encodedOldName}/rename`, {
          name: normalizedViewName
        });

        if (result.body.error) {
          arangoHelper.arangoError(
            "Failure",
            `Got unexpected server response: ${result.body.errorMessage}`
          );
          error = true;
        }
      }

      if (!error) {
        arangoHelper.arangoMessage(
          "Saving",
          `Please wait while the view is being saved. This could take some time for large views.`
        );
        const properties = pick(
          view,
          "consolidationIntervalMsec",
          "commitIntervalMsec",
          "cleanupIntervalStep",
          "links",
          "consolidationPolicy"
        );
        result = await route.patch(path, properties);
        arangoHelper.hideArangoNotifications();

        if (result.body.error) {
          arangoHelper.arangoError(
            "Failure",
            `Got unexpected server response: ${result.body.errorMessage}`
          );
        } else {
          window.sessionStorage.removeItem(oldName);
          window.sessionStorage.removeItem(`${oldName}-changed`);
          setChanged(false);

          if (view.name === oldName) {
            await mutate(path);
          } else {
            const { encoded: encodedViewName } = encodeHelper(view.name);
            let newRoute = `#view/${encodedViewName}`;
            window.App.navigate(newRoute, {
              trigger: true,
              replace: true
            });
          }

          arangoHelper.arangoNotification(
            "Success",
            `Updated View: ${view.name}`
          );
        }
      }
    } catch (e: any) {
      arangoHelper.arangoError(
        "Failure",
        `Got unexpected server response: ${e.message}`
      );
    }
  };

  return (
    <Button
      size="xs"
      colorScheme="green"
      leftIcon={<CheckIcon />}
      onClick={handleSave}
      isDisabled={disabled}
      marginRight="3"
    >
      Save view
    </Button>
  );
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
