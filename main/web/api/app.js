// server.js
require('dotenv').config();
const express = require('express');
const path = require('path');
const { Client } = require('pg');
const bcryptjs = require('bcryptjs');
const http = require('http');
const socketIo = require('socket.io');

const app = express();
const port = process.env.PORT || 3000;

app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// Servir archivos estáticos (frontend)
app.use(express.static(path.join(__dirname, 'public')));

// --- DB ---
let dbReady = false;
let db = null;

if (process.env.DATABASE_URL) {
  db = new Client({
    connectionString: process.env.DATABASE_URL,
    ssl: process.env.DB_SSL === 'true' ? { rejectUnauthorized: false } : false,
  });

  db.connect()
    .then(() => {
      console.log('Conexión a la base de datos PostgreSQL exitosa');
      dbReady = true;
    })
    .catch((err) => {
      console.error('Error al conectar a la base de datos:', err);
      dbReady = false;
    });
} else {
  console.warn('No se encontró DATABASE_URL en .env — endpoints que usan DB devolverán error hasta configurarla.');
}

// ---------- Endpoints ----------

// Crear nuevo usuario
app.post('/users', async (req, res) => {
  if (!dbReady) return res.status(500).json({ error: 'DB no disponible' });

  const { email, password } = req.body;
  if (!email || !password) return res.status(400).json({ error: 'Faltan datos' });

  try {
    const username = email;
    const hashedPassword = await bcryptjs.hash(password, 10);

    const query = 'INSERT INTO users (username, password) VALUES ($1, $2) RETURNING id';
    const result = await db.query(query, [username, hashedPassword]);
    const userId = result.rows[0].id;
    return res.status(201).json({ id: userId, email: username });
  } catch (err) {
    console.error('Error al insertar usuario:', err);
    // manejar unique violation
    if (err.code === '23505') return res.status(409).json({ error: 'Usuario ya existe' });
    return res.status(500).json({ error: 'Error al crear el usuario' });
  }
});

// Iniciar sesión
app.post('/login', async (req, res) => {
  if (!dbReady) return res.status(500).json({ error: 'DB no disponible' });

  const { email, password } = req.body;
  if (!email || !password) return res.status(400).json({ error: 'Faltan datos' });

  try {
    const query = 'SELECT id, username, password FROM users WHERE username = $1';
    const results = await db.query(query, [email]);

    if (results.rows && results.rows.length > 0) {
      const user = results.rows[0];
      const isValidPassword = await bcryptjs.compare(password, user.password);
      if (isValidPassword) {
        // Devuelvo email e id (el frontend espera email)
        return res.status(200).json({ email: user.username, id: user.id });
      } else {
        return res.status(401).json({ error: 'Usuario o contraseña incorrectos' });
      }
    } else {
      return res.status(404).json({ error: 'Usuario no encontrado' });
    }
  } catch (err) {
    console.error('Error en /login:', err);
    return res.status(500).json({ error: 'Error del servidor' });
  }
});

// Obtener detalles de usuario (frontend lo pide)
app.post('/getUserDetails', async (req, res) => {
  if (!dbReady) return res.status(500).json({ error: 'DB no disponible' });

  const { email } = req.body;
  if (!email) return res.status(400).json({ error: 'Falta email' });

  try {
    const query = 'SELECT id, username FROM users WHERE username = $1';
    const results = await db.query(query, [email]);
    if (results.rows.length > 0) {
      return res.status(200).json({ id: results.rows[0].id, email: results.rows[0].username });
    } else {
      return res.status(404).json({ error: 'Usuario no encontrado' });
    }
  } catch (err) {
    console.error('Error en /getUserDetails:', err);
    return res.status(500).json({ error: 'Error del servidor' });
  }
});

// Dispositivos: listar por userId
app.get('/devices', async (req, res) => {
  if (!dbReady) return res.status(500).json({ error: 'DB no disponible' });

  const userId = req.query.userId;
  if (!userId) return res.status(400).json({ error: 'Falta userId' });

  try {
    const query = 'SELECT id, nombre FROM devices WHERE user_id = $1 ORDER BY id';
    const results = await db.query(query, [userId]);
    return res.status(200).json(results.rows);
  } catch (err) {
    console.error('Error en GET /devices:', err);
    return res.status(500).json({ error: 'Error del servidor' });
  }
});

// Crear dispositivo
app.post('/devices', async (req, res) => {
  if (!dbReady) return res.status(500).json({ error: 'DB no disponible' });

  const { userId, nombre } = req.body;
  if (!userId || !nombre) return res.status(400).json({ error: 'Faltan datos' });

  try {
    const query = 'INSERT INTO devices (user_id, nombre) VALUES ($1, $2) RETURNING id, nombre';
    const result = await db.query(query, [userId, nombre]);
    return res.status(201).json({ id: result.rows[0].id, nombre: result.rows[0].nombre, userId });
  } catch (err) {
    console.error('Error en POST /devices:', err);
    return res.status(500).json({ error: 'Error del servidor' });
  }
});

// Eliminar dispositivo
app.delete('/devices/:id', async (req, res) => {
  if (!dbReady) return res.status(500).json({ error: 'DB no disponible' });

  const deviceId = req.params.id;
  if (!deviceId) return res.status(400).json({ error: 'Falta id' });

  try {
    const query = 'DELETE FROM devices WHERE id = $1';
    await db.query(query, [deviceId]);
    return res.sendStatus(200);
  } catch (err) {
    console.error('Error en DELETE /devices/:id', err);
    return res.status(500).json({ error: 'Error del servidor' });
  }
});

// Resto de rutas estáticas las maneja express.static (public)

// Para cualquier otra ruta no existente, devolver index (spa-friendly)
app.get('*', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Crear servidor y socket.io (opcional)
const server = http.createServer(app);
const io = socketIo(server);

io.on('connection', (socket) => {
  console.log('Cliente conectado (socket.io)');
  socket.on('disconnect', () => console.log('Cliente desconectado (socket.io)'));
});

server.listen(port, () => {
  console.log(`Servidor corriendo en http://localhost:${port}`);
});
