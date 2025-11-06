const express = require('express');
const path = require('path');

const app = express();
const PORT = 3001; // Puedes cambiarlo si lo necesitas

// Servir los archivos estáticos de la carpeta "public"
app.use(express.static(path.join(__dirname, 'public')));

// Servir index.html por defecto
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Arrancar el servidor
app.listen(PORT, () => {
  console.log(`Servidor corriendo en http://localhost:${PORT}`);
});
