import { pick } from "lodash";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";
import { FormState } from "../constants";
import { useSyncSearchViewUpdates } from "../searchAliasView/useSyncSearchViewUpdates";

export function usePatchArangoSearchView(
  view: FormState,
  oldName: string | undefined,
  setChanged: (changed: boolean) => void
) {
  useSyncSearchViewUpdates({ viewName: view.name });
  const handleSave = async () => {
    let hasError = false;
    const isNameChanged = (oldName && view.name !== oldName) || false;
    try {
      if (isNameChanged && oldName) {
        hasError = await putRenameView({ oldName, view });
      }

      if (!hasError) {
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
  const { encoded: encodedOldViewName } = encodeHelper(oldName);
  const result = await route.put(`/view/${encodedOldViewName}/rename`, {
    name: view.name.normalize()
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
  const { encoded: encodedViewName } = encodeHelper(view.name);
  const path = `/view/${encodedViewName}/properties`;
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
      let newRoute = `#view/${encodedViewName}`;
      window.App.navigate(newRoute, {
        trigger: true,
        replace: true
      });
    }
  }
  return result;
}
