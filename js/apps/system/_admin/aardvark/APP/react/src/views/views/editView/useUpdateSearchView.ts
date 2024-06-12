import { mutate } from "swr";
import {
  getApiRouteForCurrentDB,
  getCurrentDB
} from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";
import { ViewPropertiesType } from "../View.types";

type GetPropertiesType = ({
  view,
  initialView
}: {
  view: ViewPropertiesType;
  initialView: ViewPropertiesType;
}) => any;
export const useUpdateSearchView = ({
  getProperties
}: {
  getProperties: GetPropertiesType;
}) => {
  const onSave = async ({
    view,
    initialView,
    setChanged
  }: {
    view: ViewPropertiesType;
    initialView: ViewPropertiesType;
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
      setChanged,
      getProperties
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
  // normalize again here because
  // this can change from the JSON form too
  const normalizedViewName = name.normalize();
  const encodedInitialViewName = encodeHelper(initialName).encoded;
  try {
    await getCurrentDB()
      .view(encodedInitialViewName)
      .rename(normalizedViewName);
  } catch (e: any) {
    window.arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${e.response.parsedBody.errorMessage}`
    );
    isError = true;
  }
  return { isError };
};
async function patchViewProperties({
  view,
  isNameChanged,
  initialView,
  setChanged,
  getProperties
}: {
  view: ViewPropertiesType;
  isNameChanged: boolean;
  initialView: ViewPropertiesType;
  setChanged: (changed: boolean) => void;
  getProperties: GetPropertiesType;
}) {
  const encodedViewName = encodeHelper(view.name).encoded;
  const path = `/view/${encodedViewName}/properties`;
  window.arangoHelper.hideArangoNotifications();
  try {
    const result = await patchProperties({
      view,
      path,
      getProperties,
      initialView
    });
    if (result.parsedBody.error) {
      window.arangoHelper.arangoError(
        "Failure",
        `Got unexpected server response: ${result.parsedBody.errorMessage}`
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
  initialView,
  path,
  getProperties
}: {
  view: ViewPropertiesType;
  initialView: ViewPropertiesType;
  path: string;
  getProperties: GetPropertiesType;
}) {
  const route = getApiRouteForCurrentDB();

  window.arangoHelper.arangoMessage(
    "Saving",
    `Please wait while the view is being saved. This could take some time for large views.`
  );

  const properties = getProperties({
    view,
    initialView
  });
  const result = await route.patch(
    path,
    properties,
    {},
    {
      "x-arango-async": "store"
    }
  );
  const asyncId = result.headers.get("x-arango-async-id");
  window.arangoHelper.addAardvarkJob({
    id: asyncId,
    type: "view",
    desc: "Patching View",
    collection: view.name
  });

  return result;
}
