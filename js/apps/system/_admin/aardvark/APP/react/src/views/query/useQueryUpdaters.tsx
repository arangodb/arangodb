import { mutate } from "swr";
import { getCurrentDB } from "../../utils/arangoClient";
import { QueryType } from "./editor/useFetchUserSavedQueries";

export const useQueryUpdaters = ({
  queryValue,
  queryBindParams,
  savedQueries
}: {
  queryValue: string;
  queryBindParams: { [key: string]: string };
  queryName?: string;
  savedQueries?: QueryType[];
}) => {
  const onSaveAs = async (queryName: string) => {
    const newQueries = [
      ...(savedQueries || []),
      { name: queryName, value: queryValue, parameter: queryBindParams }
    ];
    await patchQueries({
      queries: newQueries,
      onSuccess: () => {
        window.arangoHelper.arangoNotification(`Saved query: ${queryName}`);
      }
    });
  };
  const onSave = async (queryName: string) => {
    await mutate("/savedQueries");
    if (queryName) {
      const newQueries = savedQueries?.map(query => {
        if (query.name === queryName) {
          return {
            name: queryName,
            value: queryValue,
            parameter: queryBindParams,
            modified_at: Date.now()
          };
        }
        return query;
      });
      await patchQueries({
        queries: newQueries || [],
        onSuccess: () => {
          window.arangoHelper.arangoNotification(`Updated query: ${queryName}`);
        }
      });
    }
  };

  const onDelete = async (queryName: string) => {
    const newQueries = savedQueries?.filter(query => query.name !== queryName);
    await patchQueries({
      queries: newQueries || [],
      onSuccess: () => {
        window.arangoHelper.arangoNotification(`Deleted query: ${queryName}`);
      }
    });
  };
  return { onSaveAs, onSave, onDelete };
};

const patchQueries = async ({
  queries,
  onSuccess
}: {
  queries: QueryType[];
  onSuccess: () => void;
}) => {
  const currentDB = getCurrentDB();
  try {
    await currentDB.request({
      method: "PATCH",
      path: `/_api/user/${encodeURIComponent(window.App.currentUser)}`,
      body: {
        extra: {
          queries
        }
      }
    });
    onSuccess();
    mutate("/savedQueries");
  } catch (e: any) {
    const message = e.message || e.response.body.errorMessage;
    window.arangoHelper.arangoError(`Could not save query. Error - ${message}`);
  }
};
