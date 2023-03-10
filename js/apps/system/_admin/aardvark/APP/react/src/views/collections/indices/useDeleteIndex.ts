import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";

export const useDeleteIndex = () => {
  const onDeleteIndex = ({
    id,
    onSuccess
  }: {
    id: string;
    onSuccess: () => void;
  }) => {
    postDeleteIndex({ id, onSuccess });
  };
  return { onDeleteIndex };
};

const handleError = () => {
  window.arangoHelper.arangoError("Could not delete index.");
};

const handleSuccess = ({
  id,
  onSuccess
}: {
  onSuccess: () => void;
  id: string;
}) => {
  window.arangoHelper.arangoNotification(`Index deleted (ID: ${id}`);
  onSuccess();
};
const postDeleteIndex = async ({
  id,
  onSuccess
}: {
  id: string;
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
        result = await getApiRouteForCurrentDB().delete(`index/${id}`);
        if (result.body.code === 200) {
          handleSuccess({ onSuccess, id });
        }
      } catch {
        handleError();
      }

      console.log(result);
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
