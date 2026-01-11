import { useSWRConfig } from "swr";
import { getCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";
import { notifySuccess } from "../../../utils/notifications";

export const useDeleteIndex = ({
  collectionName
}: {
  collectionName: string;
}) => {
  const { encoded: encodedCollectionName } = encodeHelper(collectionName);
  const { mutate } = useSWRConfig();

  const onDeleteIndex = ({
    id,
    onSuccess
  }: {
    id: string;
    onSuccess: () => void;
  }) => {
    window.arangoHelper.checkDatabasePermissions(
      function () {
        window.arangoHelper.arangoError(
          "You do not have the permissions to delete indexes in this database."
        );
      },
      async () => {
        try {
          const db = getCurrentDB();
          await db.createJob(() =>
            db.collection(encodedCollectionName).dropIndex({ id })
          );
          // Refetch index list to show updated state
          // Initial mutate
          mutate(`/collection/${encodedCollectionName}/indices`);
          // The async job may take time to complete, so poll a few times
          const pollForDeletion = (attempts: number) => {
            if (attempts > 0) {
              setTimeout(() => {
                mutate(`/collection/${encodedCollectionName}/indices`);
                pollForDeletion(attempts - 1);
              }, 1000);
            }
          };
          pollForDeletion(5); // Poll 5 times over 5 seconds
          notifySuccess(`Index deletion in progress (ID: ${id})`);
          onSuccess();
        } catch {
          window.arangoHelper.arangoError("Could not delete index.");
        }
      }
    );
  };
  return { onDeleteIndex };
};
