import { differenceWith, isEqual } from "lodash";
import { mutate } from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { ViewPropertiesType } from "./useFetchViewProperties";


export const useUpdateAliasViewProperties = () => {
  const onSave = async ({
    view,
    initialView
  }: {
    view: ViewPropertiesType;
    initialView: ViewPropertiesType;
  }) => {
    try {
      const isNameChanged = initialView.name && view.name !== initialView.name;
      let isError = false;
      if (isNameChanged) {
        isError = await putRenameView({
          initialName: initialView.name,
          name: view.name
        });
      }

      if (!isError) {
        await patchViewProperties({ view, isNameChanged, initialView });
      }
    } catch (e) {
      window.arangoHelper.arangoError(
        "Failure",
        `Got unexpected server response: ${e.message}`
      );
    }
  };

  return { onSave };
};

export const putRenameView = async ({
  initialName,
  name
}: {
  initialName: string;
  name: string;
}) => {
  let isError = false;
  const route = getApiRouteForCurrentDB();

  const result = await route.put(`/view/${initialName}/rename`, {
    name: name
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
  initialView
}: {
  view: ViewPropertiesType;
  isNameChanged: string | boolean;
  initialView: ViewPropertiesType;
}) {
  const path = `/view/${view.name}/properties`;
  const result = await patchProperties({ view, path, initialView });
  window.arangoHelper.hideArangoNotifications();

  if (result.body.error) {
    window.arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${result.body.errorMessage}`
    );
  } else {
    // setChanged(false);
    if (!isNameChanged) {
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
  const { changesAdded, changesDeleted } = getUpdatedIndexes({
    view,
    initialView
  });
  let properties = changesAdded;
  if (changesDeleted.length > 0) {
    const deletedIndexes = changesDeleted.map(index => {
      return { ...index, operation: "del" };
    });
    properties = [...changesAdded, ...deletedIndexes];
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
  const changesAdded = differenceWith(
    view.indexes,
    initialView.indexes,
    isEqual
  );
  const changesDeleted = differenceWith(
    initialView.indexes,
    view.indexes,
    isEqual
  );

  return { changesAdded, changesDeleted };
};
