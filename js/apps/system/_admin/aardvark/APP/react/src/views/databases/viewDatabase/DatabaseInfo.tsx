import { Formik } from "formik";
import React from "react";
import { AddDatabaseForm } from "../addDatabase/AddDatabaseForm";
import { DatabaseDescription, DatabaseExtraDetails } from "../Database.types";

export const DatabaseInfo = ({
  database
}: {
  database: DatabaseDescription & DatabaseExtraDetails;
}) => {
  return (
    <Formik
      initialValues={database}
      onSubmit={() => {
        // noop
      }}
    >
      <AddDatabaseForm />
    </Formik>
  );
};
