import { getCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";
import { notifySuccess } from "../../../utils/notifications";

export const useDeleteIndex = ({
  collectionId,
  collectionName
}: {
  collectionId: string;
  collectionName: string;
}) => {
  const onDeleteIndex = ({
    id,
    onSuccess
  }: {
    id: string;
    onSuccess: () => void;
  }) => {
    postDeleteIndex({ id, onSuccess, collectionId, collectionName });
  };
  return { onDeleteIndex };
};

const handleError = () => {
  window.arangoHelper.arangoError("Could not delete index.");
};

const handleSuccess = ({
  id,
  asyncId,
  collectionId,
  onSuccess
}: {
  id: string;
  collectionId: string;
  asyncId: string;
  onSuccess: () => void;
}) => {
  window.arangoHelper.addAardvarkJob({
    id: asyncId,
    type: "index",
    desc: "Removing Index",
    collection: collectionId
  });
  notifySuccess(`Index deletion in progress (ID: ${id})`);
  onSuccess();
};
const postDeleteIndex = async ({
  id,
  collectionId,
  collectionName,
  onSuccess
}: {
  id: string;
  collectionId: string;
  collectionName: string;
  onSuccess: () => void;
}) => {
  window.arangoHelper.checkDatabasePermissions(
    function () {
      window.arangoHelper.arangoError(
        "You do not have the permissions to delete indexes in this database."
      );
    },
    async () => {
      let result;
      try {
        const db = getCurrentDB();
        const { encoded: encodedCollectionName } = encodeHelper(collectionName);
        result = await db.createJob(() =>
          db.collection(encodedCollectionName).dropIndex({ id })
        );

        handleSuccess({ onSuccess, id, asyncId: result.id, collectionId });
      } catch {
        handleError();
      }
    }
  );
};
