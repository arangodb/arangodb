import './SlideInMenuStyle.css';

export const SlideInMenu = ({ children }) => {
  return (
    <div>
      <label for="menu-control" class="headerButton">
        <i class="fa fa-bars fa-stack-1x"></i>
      </label>

      <input type="checkbox" id="menu-control" class="menu-control" />
    
      <aside class="sidebar">
        {children}
        <label for="menu-control" class="sidebar__close"></label>
      </aside>
    </div>
  );
}

//export default SlideInMenu;
