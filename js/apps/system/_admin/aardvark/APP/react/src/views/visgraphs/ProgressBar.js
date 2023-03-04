/* global arangoHelper, $ */
import React, { useState, useRef } from 'react';
import "./progressBar.css";

  export const ProgressBar = () => {

  return (
    <div id="graphViewerLoadingBar">
        <div class="graphViewerOuterBorder">
            <div id="graphViewerLoadingText">0%</div>
            <div id="graphViewerBorder">
                <div id="graphViewerProgressBar"></div>
            </div>
        </div>
    </div>
  )
};
