import { CheckIcon } from "@chakra-ui/icons";
import { Button } from "@chakra-ui/react";
import { pick } from "lodash";
import React from "react";
import { mutate } from "swr";
import { getApiRouteForCurrentDB } from "../../utils/arangoClient";
import { FormState } from "./constants";

type SaveButtonProps = {
  view: FormState;
  disabled?: boolean;
  oldName?: string;
  setChanged: (changed: boolean) => void;
};

export const SaveArangoSearchViewButton = ({
  disabled,
  view,
  oldName,
  setChanged
}: SaveButtonProps) => {
  const handleSave = useHandleSave(view, oldName, setChanged);

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

function useHandleSave(
  view: FormState,
  oldName: string | undefined,
  setChanged: (changed: boolean) => void
) {
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
          window.arangoHelper.arangoError(
            "Failure",
            `Got unexpected server response: ${result.body.errorMessage}`
          );
          error = true;
        }
      }

      if (!error) {
        window.arangoHelper.arangoMessage(
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
        window.arangoHelper.hideArangoNotifications();

        if (result.body.error) {
          window.arangoHelper.arangoError(
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

          window.arangoHelper.arangoNotification(
            "Success",
            `Updated View: ${view.name}`
          );
        }
      }
    } catch (e: any) {
      window.arangoHelper.arangoError(
        "Failure",
        `Got unexpected server response: ${e.message}`
      );
    }
  };
  return handleSave;
}
