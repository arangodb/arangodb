import { CheckIcon } from "@chakra-ui/icons";
import { Button } from "@chakra-ui/react";
import { pick } from "lodash";
import React, { useEffect } from "react";
import { useSWRConfig } from "swr";
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
  useSyncPatchViewJob({ view });
  const handleSave = async () => {
    let hasError = false;
    const isNameChanged = (oldName && view.name !== oldName) || false;
    try {
      if (isNameChanged && oldName) {
        hasError = await putRenameView({ oldName, view });
      }

      if (!hasError) {
        console.log("patch!");
        await patchViewProperties({ view, oldName, setChanged });
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

async function putRenameView({
  oldName,
  view
}: {
  oldName: string;
  view: FormState;
}) {
  const route = getApiRouteForCurrentDB();
  let error = false;
  const result = await route.put(`/view/${oldName}/rename`, {
    name: view.name
  });

  if (result.body.error) {
    window.arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${result.body.errorMessage}`
    );
    error = true;
  }
  return error;
}

async function patchViewProperties({
  view,
  oldName,
  setChanged
}: {
  view: FormState;
  oldName: string | undefined;
  setChanged: (changed: boolean) => void;
}) {
  const path = `/view/${view.name}/properties`;

  const route = getApiRouteForCurrentDB();
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
  const result = await route.patch(
    path,
    properties,
    {},
    {
      "x-arango-async": "store"
    }
  );
  const asyncId = result.headers["x-arango-async-id"];
  window.arangoHelper.addAardvarkJob({
    id: asyncId,
    type: "view",
    desc: "Patching View",
    collection: view.name
  });
  if (result.body.error) {
    window.arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${result.body.errorMessage}`
    );
  } else {
    oldName && window.sessionStorage.removeItem(oldName);
    oldName && window.sessionStorage.removeItem(`${oldName}-changed`);
    setChanged(false);

    if (view.name !== oldName) {
      let newRoute = `#view/${view.name}`;
      window.App.navigate(newRoute, {
        trigger: true,
        replace: true
      });
    }
  }
  return result;
}
const useSyncPatchViewJob = ({ view }: { view: FormState }) => {
  const { mutate } = useSWRConfig();

  const checkState = function (
    error: boolean,
    jobs: { id: string; collection: string }[]
  ) {
    if (error) {
      window.arangoHelper.arangoError("Jobs", "Could not read pending jobs.");
      
    } else {
      const foundJob = jobs.find(locked => locked.collection === view.name);
      if (foundJob) {
        const path = `/view/${view.name}/properties`;
        mutate(path);
        window.arangoHelper.arangoNotification(
          "Success",
          `Updated View: ${view.name}`
        );
        window.arangoHelper.deleteAardvarkJob(foundJob.id);
      }
    }
  };
  useEffect(() => {
    window.arangoHelper.syncAndReturnUnfinishedAardvarkJobs("view", checkState);

    let interval = window.setInterval(() => {
      window.arangoHelper.getAardvarkJobs(checkState);
    }, 10000);
    return () => window.clearInterval(interval);
    // disabled because function creation can cause re-render, and this only needs to run on mount
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
};
