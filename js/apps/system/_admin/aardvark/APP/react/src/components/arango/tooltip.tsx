import React from "react";

type TooltipProps = {
  icon: string;
  msg: string;
  placement: string;
  dataTippy?: string;
};

const Tooltip: React.FC<TooltipProps> = props => {
  return (
    <div className="tooltipDiv">
      <a
        className="index-tooltip"
        data-toggle="tooltip"
        data-placement={props.placement}
        data-tippy={props.dataTippy}
        data-original-title={props.msg}
      >
        <span className={`arangoicon" ${props.icon}`} />
      </a>
    </div>
  );
};
export default Tooltip;
