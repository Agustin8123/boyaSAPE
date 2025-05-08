const express = require('express');

const { Client } = require('pg');
const bcryptjs = require('bcryptjs');

const http = require('http');
const socketIo = require('socket.io');

require('dotenv').config();
const app = express();
const port = 3000;

app.use(express.json()); // Para recibir datos JSON

const db = new Client({
    connectionString: Postgres.DATABASE_URL, 
    ssl: { rejectUnauthorized: false } // Asegura conexión segura
});

// Verificar conexión
db.connect()
  .then(() => console.log('Conexión a la base de datos PostgreSQL exitosa'))
  .catch(err => console.error('Error al conectar a la base de datos:', err));

// Crear nuevo usuario
app.post('/users', async (req, res) => {
    const { username, password} = req.body;
    const hashedPassword = await bcryptjs.hash(password, 10);

    const query = 'INSERT INTO users (username, password, image, description) VALUES ($1, $2, $3, $4) RETURNING id';
    db.query(query, [username, hashedPassword || null], (err, result) => {
        if (err) {
            console.error("Error al insertar usuario:", err);
            res.status(500).json({ error: 'Error al crear el usuario' });
        } else {
            const userId = result.rows[0].id;
            res.status(201).json({ id: userId, username });
        }
    });
});

// Iniciar sesión con un usuario existente
app.post('/login', (req, res) => {
    const { username, password } = req.body;

    const query = 'SELECT * FROM users WHERE username = $1';
    db.query(query, [username], async (err, results) => {
        if (err) {
            console.error('Error al buscar el usuario:', err);
            return res.status(500).json('Error al buscar el usuario');
        }

        console.log('Resultados de la consulta:', results);
        if (results.rows && results.rows.length > 0) {
            const user = results.rows[0];
            const isValidPassword = await bcryptjs.compare(password, user.password);

            if (isValidPassword) {
                console.log('Login exitoso para usuario:', username);
                return res.status(200).json({ username: user.username });
            } else {
                console.log('Contraseña incorrecta para usuario:', username);
                return res.status(401).json('Usuario o contraseña incorrectos');
            }
        } else {
            console.log('Usuario no encontrado:', username);
            return res.status(404).json('Usuario no encontrado');
        }
    });
});
;


app.use(express.static(path.join(__dirname, 'public')));

app.get('/:page?', (req, res) => {
    let page = req.params.page || 'index'; // Si no hay parámetro, usar 'inicio'
    let filePath = path.join(__dirname, '../public/html', `${page}.html`);

    res.sendFile(filePath, (err) => {
        if (err) {
            res.sendFile(path.join(__dirname, 'public', 'error.html')); // Si no existe, cargar error.html
        }
    });
});

// Crear el servidor
const server = app.listen(port, () => {
    console.log(`Servidor corriendo en http://localhost:${port}`);
});

// Iniciar Socket.IO en el servidor
const io = socketIo(server);

// Cuando un cliente se conecta
io.on('connection', (socket) => {
  console.log('Un cliente se ha conectado');
  
  // Aquí podrías hacer otras configuraciones, como emitir un mensaje de bienvenida

  // Cuando un cliente se desconecta
  socket.on('disconnect', () => {
    console.log('Un cliente se ha desconectado');
  });
});
// Iniciar el servidor