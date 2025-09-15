// public/scripts/scripts.js
// Manejo básico de sesiones, usuarios y dispositivos
let users = {};
let activeUser = null;
let dispositivos = [];

// --- Helpers cookies ---
function encodePassword(password) {
  return btoa(password);
}
function decodePassword(encodedPassword) {
  try { return atob(encodedPassword); } catch (e) { return ''; }
}
function setCookie(name, value, days) {
  let expires = '';
  if (days) {
    const date = new Date();
    date.setTime(date.getTime() + days * 24 * 60 * 60 * 1000);
    expires = '; expires=' + date.toUTCString();
  }
  document.cookie = `${name}=${value || ''}${expires}; path=/; SameSite=Lax`;
}
function getCookie(name) {
  const value = `; ${document.cookie}`;
  const parts = value.split(`; ${name}=`);
  if (parts.length === 2) return parts.pop().split(';').shift();
  return null;
}
function deleteCookie(name) {
  document.cookie = `${name}=; Path=/; Expires=Thu, 01 Jan 1970 00:00:01 GMT; SameSite=Lax`;
}

// --- Session ---
function saveSession(email, password, rememberMe) {
  if (rememberMe) {
    setCookie('email', email, 7);
    setCookie('password', encodePassword(password), 7);
  } else {
    deleteCookie('email');
    deleteCookie('password');
  }
}

function checkRememberedUser() {
  const email = getCookie('email');
  const encodedPassword = getCookie('password');
  if (email && encodedPassword) {
    document.getElementById('emailInput').value = email;
    document.getElementById('passwordInput').value = decodePassword(encodedPassword);
    document.getElementById('rememberMe').checked = true;
    loginUser();
  }
}

// --- Validaciones ---
function validarEmail(email) {
  const regex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
  return regex.test(email);
}
function validarPassword(password) {
  const regex = /^(?=.*[A-Z])(?=.*\d)(?=.*[!@#$%^&*_\-+=?]).{8,}$/;
  return regex.test(password);
}

// --- Login / Registro ---
function loginUser() {
  const email = document.getElementById('emailInput').value.trim();
  const password = document.getElementById('passwordInput').value.trim();
  const rememberMe = document.getElementById('rememberMe').checked;

  if (!email || !password) {
    alert('Ingrese email y contraseña.');
    return;
  }

  fetch('/login', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ email, password })
  })
    .then(r => r.json())
    .then(data => {
      if (data && data.email) {
        // data.id viene del servidor
        users[email] = { id: data.id };
        setActiveUser(email);
        saveSession(email, password, rememberMe);
      } else if (data && data.error) {
        alert(data.error);
      } else {
        alert('Usuario o contraseña incorrectos.');
      }
    })
    .catch(() => alert('Error de conexión'));
}

function addNewUser() {
  const email = document.getElementById('emailInput').value.trim();
  const password = document.getElementById('passwordInput').value.trim();

  if (!email || !password) {
    alert('Por favor complete email y contraseña.');
    return;
  }
  if (!validarEmail(email)) {
    alert('Ingrese un email válido (ejemplo@correo.com).');
    return;
  }
  if (!validarPassword(password)) {
    alert('La contraseña debe tener mínimo 8 caracteres, una mayúscula, un número y un símbolo.');
    return;
  }

  fetch('/users', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ email, password }),
  })
    .then(r => r.json())
    .then(data => {
      if (data && data.id) {
        users[email] = { id: data.id };
        setActiveUser(email);
        alert('Cuenta creada correctamente 🎉');
      } else if (data && data.error) {
        alert(data.error);
      } else {
        alert('Error al crear usuario.');
      }
    })
    .catch(() => alert('Hubo un error al crear el usuario.'));
}

// --- Set active user: obtiene detalles si falta id ---
function setActiveUser(email) {
  // si ya tenemos id en memory
  if (users[email] && users[email].id) {
    activeUser = email;
    menuSesion();
    showMainUIForLoggedUser();
    loadDispositivos();
    return;
  }

  // pedir detalles al backend
  fetch('/getUserDetails', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ email }),
  })
    .then(r => r.json())
    .then(data => {
      if (data && data.id) {
        users[email] = { id: data.id };
        activeUser = email;
        setCookie('userID', data.id, 7);
        menuSesion();
        showMainUIForLoggedUser();
        loadDispositivos();
      } else {
        alert('Usuario no encontrado.');
      }
    })
    .catch(() => alert('Error al obtener los detalles del usuario.'));
}

// --- UI helpers ---
function menuSesion() {
  document.getElementById('btnCuenta').style.display = 'inline-block';
}

function showMainUIForLoggedUser() {
  document.getElementById('sesion').style.display = 'none';
  document.getElementById('dispositivos').style.display = 'block';
}

function hideMenus(...ids) {
  ids.forEach(id => {
    const el = document.getElementById(id);
    if (el) el.style.display = 'none';
  });
}

// --- Dispositivos ---
function renderDispositivos() {
  const lista = document.getElementById('dispLista');
  if (!lista) return;
  if (!dispositivos || dispositivos.length === 0) {
    lista.innerHTML = '<p>No tenés dispositivos aún.</p>';
    return;
  }

  lista.innerHTML = '<ul>' + dispositivos.map((d) =>
    `<li style="display:flex;gap:8px;align-items:center;">
      <button onclick="menuConfigDisp(${d.id})">${escapeHtml(d.nombre)}</button>
      <button style="width:40px;height:26px;background:red;color:white;border:none;border-radius:4px;" onclick="removeDispositivo(${d.id});">X</button>
    </li>`
  ).join('') + '</ul>';
}

function loadDispositivos() {
  if (!activeUser || !users[activeUser]?.id) return;
  fetch(`/devices?userId=${users[activeUser].id}`)
    .then(r => r.json())
    .then(data => {
      dispositivos = Array.isArray(data) ? data : [];
      renderDispositivos();
    })
    .catch(err => {
      console.error('Error al cargar dispositivos:', err);
      alert('No se pudieron cargar los dispositivos.');
    });
}

function addDispositivo() {
  if (!activeUser || !users[activeUser]?.id) {
    alert('Primero inicia sesión.');
    return;
  }
  const nombre = prompt('Nombre del nuevo dispositivo:');
  if (!nombre) return;

  const nuevo = { userId: users[activeUser].id, nombre };
  fetch('/devices', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(nuevo)
  })
    .then(r => r.json())
    .then(data => {
      if (data && data.id) {
        dispositivos.push({ id: data.id, nombre: data.nombre });
        renderDispositivos();
      } else {
        alert('No se pudo crear el dispositivo.');
      }
    })
    .catch(err => {
      console.error('Error al guardar dispositivo:', err);
      alert('No se pudo guardar el dispositivo.');
    });
}

function removeDispositivo(id) {
  if (!confirm('¿Seguro que querés eliminar este dispositivo?')) return;
  fetch(`/devices/${id}`, { method: 'DELETE' })
    .then(r => {
      if (r.ok) {
        dispositivos = dispositivos.filter(d => d.id !== id);
        renderDispositivos();
      } else {
        alert('Error al eliminar dispositivo.');
      }
    })
    .catch(err => {
      console.error('Error al eliminar dispositivo:', err);
      alert('No se pudo eliminar el dispositivo.');
    });
}

// placeholder para configuración del dispositivo seleccionado
function menuConfigDisp(deviceId) {
  alert('Abrir configuración del dispositivo ID: ' + deviceId);
  // aquí podés mostrar la sección de configuración y cargar datos del dispositivo
}

function menuConfigDisp2() {
  // volver a lista de dispositivos
  document.getElementById('configuracion').style.display = 'none';
  document.getElementById('dispositivos').style.display = 'block';
}

// Logout
function logoutUser() {
  deleteCookie('userID');
  deleteCookie('email');
  deleteCookie('password');
  activeUser = null;
  dispositivos = [];
  renderDispositivos();
  hideMenus('configuracion');
  document.getElementById('sesion').style.display = 'block';
  document.getElementById('dispositivos').style.display = 'none';
  document.getElementById('btnCuenta').style.display = 'none';
  alert('Sesión cerrada correctamente.');
}

// --- Utiles ---
function escapeHtml(text) {
  if (!text) return '';
  return text.replace(/[&<>"']/g, function (m) {
    return ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[m];
  });
}

// Init
window.addEventListener('load', () => {
  checkRememberedUser();
});
