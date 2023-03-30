import React, { useState } from "react";
import { getApiRouteForCurrentDB } from "../../utils/arangoClient";
import { FormState } from "./constants";
import { pick } from "lodash";
import { mutate } from "swr";
import { DeleteViewModal } from "./DeleteViewModal";
import { CheckIcon, DeleteIcon } from "@chakra-ui/icons";
import { Button } from "@chakra-ui/react";

declare var arangoHelper: { [key: string]: any };
declare var window: { [key: string]: any };

type DeleteButtonWrapProps = {
  view: FormState;
  disabled?: boolean
};

type SaveButtonProps = {
  view: FormState
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
    const path = `/view/${view.name}/properties`;

    try {
      if (oldName && view.name !== oldName) {
        result = await route.put(`/view/${oldName}/rename`, {
          name: view.name
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
            let newRoute = `#view/${view.name}`;
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
    } catch (e) {
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
  )
};

export const DeleteButtonWrap = ({ view, disabled }: DeleteButtonWrapProps) => {
  const [show, setShow] = useState(false);


  const handleDelete = async () => {
    try {
      const result = await getApiRouteForCurrentDB().delete(
        `/view/${view.name}`
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
    } catch (e) {
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
