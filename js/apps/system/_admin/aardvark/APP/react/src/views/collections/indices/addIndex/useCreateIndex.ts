import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";

const handleError = (error: { errorMessage: string }) => {
  if (error.errorMessage) {
    window.arangoHelper.arangoError("Index error", error.errorMessage);
  } else {
    window.arangoHelper.arangoError("Index error", "Could not create index.");
  }
};

const handleSuccess = (onSuccess: () => void) => {
  window.arangoHelper.arangoNotification(
    "Index",
    "Creation in progress. This may take a while."
  );
  onSuccess();
};

export const useCreateIndex = <TValues extends unknown>() => {
  const {
    collectionName,
    collectionId,
    onCloseForm
  } = useCollectionIndicesContext();

  const onCreate = async (values: TValues) => {
    window.arangoHelper.checkDatabasePermissions(
      function() {
        window.arangoHelper.arangoError(
          "You do not have the permissions to create indexes in this database."
        );
      },
      async () => {
        let result;
        try {
          result = await getApiRouteForCurrentDB().post(
            `index`,
            values,
            `collection=${collectionName}`,
            {
              "x-arango-async": "store"
            }
          );
          const syncId = result.headers["x-arango-async-id"];
          if (result.statusCode === 202 && syncId) {
            window.arangoHelper.addAardvarkJob({
              id: syncId,
              type: "index",
              desc: "Creating Index",
              collection: collectionId
            });
            handleSuccess(onCloseForm);
          }
          if (result.body.code === 201) {
            handleSuccess(onCloseForm);
          }
        } catch (error: any) {
          handleError(error.response.body);
        }
      }
    );
  };
  return { onCreate };
};
