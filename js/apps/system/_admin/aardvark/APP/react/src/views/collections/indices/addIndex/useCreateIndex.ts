import { getCurrentDB } from "../../../../utils/arangoClient";
import { encodeHelper } from "../../../../utils/encodeHelper";
import { notifyError, notifySuccess } from "../../../../utils/notifications";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";

const handleError = (error: { errorMessage: string }) => {
  if (error.errorMessage) {
    notifyError(`Index creation failed: ${error.errorMessage}`);
  } else {
    notifyError("Index creation failed.");
  }
};

const handleSuccess = (onSuccess: () => void) => {
  notifySuccess("Index creation in progress, this may take a while.");
  onSuccess();
};

export const useCreateIndex = <
  TValues extends { [key: string]: unknown }
>() => {
  const { collectionName, collectionId, onCloseForm } =
    useCollectionIndicesContext();
  const { encoded: encodedCollectionName } = encodeHelper(collectionName);
  const onCreate = async (values: TValues) => {
    window.arangoHelper.checkDatabasePermissions(
      function () {
        window.arangoHelper.arangoError(
          "You do not have the permissions to create indexes in this database."
        );
      },
      async () => {
        let job;
        try {
          const db = getCurrentDB();
          job = await db.createJob(() =>
            db.collection(encodedCollectionName).ensureIndex({
              ...values,
              name: values.name ? String(values.name)?.normalize() : undefined
            } as any)
          );
          window.arangoHelper.addAardvarkJob({
            id: job.id,
            type: "index",
            desc: "Creating Index",
            collection: collectionId
          });
          handleSuccess(onCloseForm);
        } catch (error: any) {
          handleError(error.response.parsedBody);
        }
      }
    );
  };
  return { onCreate };
};
