  const form = document.querySelector('form');
  form.addEventListener('submit', async (e) => {
    e.preventDefault();
    const response = await fetch(form.action, {
      method: form.method,
      body: new FormData(form),
      headers: { 'Accept': 'application/json' }
    });
    if (response.ok) {
      alert('✅ Tu pedido fue enviado correctamente. Te contactaremos pronto.');
      form.reset();
    } else {
      alert('❌ Ocurrió un error. Intentá nuevamente.');
    }
  });