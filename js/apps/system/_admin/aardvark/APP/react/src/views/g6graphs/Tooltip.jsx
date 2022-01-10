import { InfoCircleOutlined } from '@ant-design/icons';
import ReactTooltip from 'react-tooltip';

const Tooltip = ({text}) => {

  return <>
    <InfoCircleOutlined data-tip={text} data-place="left" />
    <ReactTooltip />
  </>;
}

export default Tooltip;
