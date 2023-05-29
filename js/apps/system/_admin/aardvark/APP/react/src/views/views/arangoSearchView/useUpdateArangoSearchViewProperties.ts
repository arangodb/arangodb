import { mutate } from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";
import { ArangoSearchViewPropertiesType } from "../searchView.types";

export const useUpdateArangoSearchViewProperties = () => {
  const onSave = async ({
    view,
    initialView,
    setChanged
  }: {
    view: ArangoSearchViewPropertiesType;
    initialView: ArangoSearchViewPropertiesType;
    setChanged: (changed: boolean) => void;
  }) => {
    const isNameChanged =
      (initialView.name && view.name !== initialView.name) || false;
    if (isNameChanged) {
      const { isError } = await putRenameView({
        initialName: initialView.name,
        name: view.name
      });
      if (isError) return;
    }
    await patchViewProperties({
      view,
      isNameChanged,
      initialView,
      setChanged
    });
  };

  return { onSave };
};

const putRenameView = async ({
  initialName,
  name
}: {
  initialName: string;
  name: string;
}) => {
  let isError = false;
  const route = getApiRouteForCurrentDB();
  // normalize again here because
  // this can change from the JSON form too
  const normalizedViewName = name.normalize();
  const encodedInitialViewName = encodeHelper(initialName).encoded;
  const result = await route.put(`/view/${encodedInitialViewName}/rename`, {
    name: normalizedViewName
  });
  if (result.body.error) {
    window.arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${result.body.errorMessage}`
    );
    isError = true;
  }
  return { isError };
};
async function patchViewProperties({
  view,
  isNameChanged,
  initialView,
  setChanged
}: {
  view: ArangoSearchViewPropertiesType;
  isNameChanged: boolean;
  initialView: ArangoSearchViewPropertiesType;
  setChanged: (changed: boolean) => void;
}) {
  const encodedViewName = encodeHelper(view.name).encoded;
  const path = `/view/${encodedViewName}/properties`;
  window.arangoHelper.hideArangoNotifications();
  try {
    const result = await patchProperties({ view, path, initialView });
    if (result.body.error) {
      window.arangoHelper.arangoError(
        "Failure",
        `Got unexpected server response: ${result.body.errorMessage}`
      );
    } else {
      window.sessionStorage.removeItem(`${initialView.name}-changed`);
      window.sessionStorage.removeItem(`${initialView.name}`);
      
      if (!isNameChanged) {
        await mutate(path);
        setChanged(false);
      } else {
        const { encoded: encodedViewName } = encodeHelper(view.name);
        let newRoute = `#view/${encodedViewName}`;
        window.App.navigate(newRoute, {
          trigger: true,
          replace: true
        });
      }
    }
  } catch (error: any) {
    window.arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${error.message}`
    );
  }
}

async function patchProperties({
  view,
  path
}: {
  view: ArangoSearchViewPropertiesType;
  initialView: ArangoSearchViewPropertiesType;
  path: string;
}) {
  const route = getApiRouteForCurrentDB();

  window.arangoHelper.arangoMessage(
    "Saving",
    `Please wait while the view is being saved. This could take some time for large views.`
  );

  const result = await route.patch(
    path,
    { ...view },
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

  return result;
}
