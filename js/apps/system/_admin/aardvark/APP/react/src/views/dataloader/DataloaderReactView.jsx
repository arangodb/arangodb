/* global arangoHelper, arangoFetch, frontendConfig, $ */
import React, { useEffect, useState } from 'react';
import FetchData from "./FetchData.js";
import "react-data-table-component-extensions/dist/index.css";

const DataloaderReactView = () => {
  return (
    <>
      <FetchData urlValue="http://hn.algolia.com/api/v1/search?query=arango" />
    </>
  );
};

window.DataloaderReactView = DataloaderReactView;
