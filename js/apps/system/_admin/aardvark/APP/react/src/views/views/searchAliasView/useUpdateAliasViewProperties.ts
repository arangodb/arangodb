import { differenceWith, isEqual } from "lodash";
import { mutate } from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";
import { ViewPropertiesType } from "./useFetchViewProperties";

export const useUpdateAliasViewProperties = ({
  setChanged
}: {
  setChanged: (changed: boolean) => void;
}) => {
  const onSave = async ({
    view,
    initialView
  }: {
    view: ViewPropertiesType;
    initialView: ViewPropertiesType;
  }) => {
    try {
      const isNameChanged =
        (initialView.name && view.name !== initialView.name) || false;
      let isError = false;
      if (isNameChanged) {
        isError = await putRenameView({
          initialName: initialView.name,
          name: view.name
        });
      }

      if (!isError) {
        await patchViewProperties({
          view,
          isNameChanged,
          initialView,
          setChanged
        });
      }
    } catch (e: any) {
      window.arangoHelper.arangoError(
        "Failure",
        `Got unexpected server response: ${e.message}`
      );
    }
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
  return isError;
};
async function patchViewProperties({
  view,
  isNameChanged,
  initialView,
  setChanged
}: {
  view: ViewPropertiesType;
  isNameChanged: boolean;
  initialView: ViewPropertiesType;
  setChanged: (changed: boolean) => void;
}) {
  const encodedViewName = encodeHelper(view.name).encoded;
  const path = `/view/${encodedViewName}/properties`;
  window.arangoHelper.hideArangoNotifications();
  const result = await patchProperties({ view, path, initialView });

  if (result.body.error) {
    window.arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${result.body.errorMessage}`
    );
  } else {
    setChanged(false);
    window.sessionStorage.removeItem(`${initialView.name}-changed`);
    window.sessionStorage.removeItem(`${initialView.name}`);

    if (!isNameChanged) {
      await mutate(path);
    } else {
      const { encoded: encodedViewName } = encodeHelper(view.name);
      let newRoute = `#view/${encodedViewName}`;
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

async function patchProperties({
  view,
  path,
  initialView
}: {
  view: ViewPropertiesType;
  initialView: ViewPropertiesType;
  path: string;
}) {
  const route = getApiRouteForCurrentDB();

  window.arangoHelper.arangoMessage(
    "Saving",
    `Please wait while the view is being saved. This could take some time for large views.`
  );
  const { addedChanges, deletedChanges } = getUpdatedIndexes({
    view,
    initialView
  });
  let properties = addedChanges;
  if (deletedChanges.length > 0) {
    const deletedIndexes = deletedChanges.map(index => {
      return { ...index, operation: "del" };
    });
    properties = [...addedChanges, ...deletedIndexes];
  }
  return await route.patch(path, { indexes: properties });
}

export const getUpdatedIndexes = ({
  view,
  initialView
}: {
  view: ViewPropertiesType;
  initialView: ViewPropertiesType;
}) => {
  const addedChanges = differenceWith(
    view.indexes,
    initialView.indexes,
    isEqual
  );
  const deletedChanges = differenceWith(
    initialView.indexes,
    view.indexes,
    isEqual
  );

  return { addedChanges, deletedChanges };
};
