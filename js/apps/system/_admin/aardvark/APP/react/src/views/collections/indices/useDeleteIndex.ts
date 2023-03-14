import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";

export const useDeleteIndex = ({ collectionId }: { collectionId: string }) => {
  const onDeleteIndex = ({
    id,
    onSuccess
  }: {
    id: string;
    onSuccess: () => void;
  }) => {
    postDeleteIndex({ id, onSuccess, collectionId });
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
  window.arangoHelper.arangoNotification(
    `Index deletion in progress (ID: ${id}`
  );
  onSuccess();
};
const postDeleteIndex = async ({
  id,
  collectionId,
  onSuccess
}: {
  id: string;
  collectionId: string;
  onSuccess: () => void;
}) => {
  window.arangoHelper.checkDatabasePermissions(
    function() {
      window.arangoHelper.arangoError(
        "You do not have permission to delete indexes in this database."
      );
    },
    async () => {
      let result;
      try {
        result = await getApiRouteForCurrentDB().delete(
          `index/${id}`,
          undefined,
          {
            "x-arango-async": "store"
          }
        );
        const asyncId = result.headers["x-arango-async-id"] as string;

        if (asyncId) {
          handleSuccess({ onSuccess, id, asyncId, collectionId });
          return;
        }
        handleError();
      } catch {
        handleError();
      }
      // $.ajax({
      //   cache: false,
      //   type: 'DELETE',
      //   url: arangoHelper.databaseUrl('/_api/index/' + encodeURIComponent(self.get('name')) + '/' + encodeURIComponent(id)),
      //   headers: {
      //     'x-arango-async': 'store'
      //   },
      //   success: function (data, textStatus, xhr) {
      //     if (xhr.getResponseHeader('x-arango-async-id')) {
      //       window.arangoHelper.addAardvarkJob({
      //         id: xhr.getResponseHeader('x-arango-async-id'),
      //         type: 'index',
      //         desc: 'Removing Index',
      //         collection: self.get('id')
      //       });
      //       callback(false, data);
      //     } else {
      //       callback(true, data);
      //     }
      //   },
      //   error: function (data) {
      //     callback(true, data);
      //   }
      // });
    }
  );
};
