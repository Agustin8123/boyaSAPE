    const form = document.getElementById('form-comentario');
    const carrusel = document.getElementById('carrusel');

    form.addEventListener('submit', (e) => {
      e.preventDefault();

      const nombre = document.getElementById('nombreUser').value;
      const mensaje = document.getElementById('mensajeUser').value;

      const card = document.createElement('div');
      card.className = 'coment-card';
      card.innerHTML = `<p>"${mensaje}"</p><h4>— ${nombre}</h4>`;

      carrusel.appendChild(card);
      form.reset();
      card.scrollIntoView({ behavior: "smooth", inline: "center" });
    });