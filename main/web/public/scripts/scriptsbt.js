

function toggleVisibility(elementId, displayType = 'block') {
    const element = document.getElementById(elementId);
    if (element) {
        // Alternar entre 'block' y 'none'
        element.style.display = (element.style.display === displayType) ? 'none' : displayType;
    } else {
        console.error(`No se encontró el elemento con id "${elementId}".`);
    }
}

function hideMenus(...menuIds) {
    menuIds.forEach(menuId => {
        const menu = document.getElementById(menuId);
        if (menu && menu.style.display !== 'none') {
            menu.style.display = 'none';
        }
    });
}



function menuConfigDisp() {
    hideMenus('dispositivos');
    toggleVisibility('configuracion', 'block');
}

function menuConfigDisp2() {
    hideMenus('configuracion');
    toggleVisibility('dispositivos', 'block');
}

function menuModelo() {
    toggleVisibility('modelo', 'block');
}

function menuBoya() {
    toggleVisibility('boya', 'block');
    hideMenus('modelo');
}