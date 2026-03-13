import { useEffect, useRef } from "react";
import { useSWRConfig } from "swr";
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
  const { collectionName, onCloseForm } = useCollectionIndicesContext();
  const { encoded: encodedCollectionName } = encodeHelper(collectionName);
  const { mutate } = useSWRConfig();
  const timeoutIds = useRef<ReturnType<typeof setTimeout>[]>([]);

  // Clear all pending timeouts on unmount
  useEffect(() => {
    return () => {
      timeoutIds.current.forEach(clearTimeout);
    };
  }, []);

  const onCreate = async (values: TValues) => {
    window.arangoHelper.checkDatabasePermissions(
      function () {
        window.arangoHelper.arangoError(
          "You do not have the permissions to create indexes in this database."
        );
      },
      async () => {
        try {
          const db = getCurrentDB();
          await db.createJob(() =>
            db.collection(encodedCollectionName).ensureIndex({
              ...values,
              name: values.name ? String(values.name)?.normalize() : undefined
            } as any)
          );
          // Refetch index list to show the in-progress index with progress indicator
          // Initial mutate
          mutate(`/collection/${encodedCollectionName}/indices`);
          // The async job may take time to start, so poll a few times to catch the index
          const pollForIndex = (attempts: number) => {
            if (attempts > 0) {
              const id = setTimeout(() => {
                mutate(`/collection/${encodedCollectionName}/indices`);
                pollForIndex(attempts - 1);
              }, 1000);
              timeoutIds.current.push(id);
            }
          };
          pollForIndex(5); // Poll 5 times over 5 seconds
          handleSuccess(onCloseForm);
        } catch (error: any) {
          handleError(error.response.parsedBody);
        }
      }
    );
  };
  return { onCreate };
};
