'use strict';

let checkingRelease = false;

async function publishedRelease(signal) {
  const manifestUrl = new URL('manifest.json', location.href);
  const response = await fetch(manifestUrl.href, { cache: 'no-store', signal });
  if (!response.ok) throw new Error('A publicação do firmware está indisponível. Tente verificar novamente.');
  const manifest = await response.json();
  const build = manifest?.builds?.find(entry => entry.chipFamily === 'ESP32');
  if (typeof manifest?.version !== 'string' || !manifest.version.trim() ||
      manifest.version === 'automatic' || !build?.parts?.length) {
    throw new Error('A publicação do firmware ainda não está pronta para instalar.');
  }
  for (const part of build.parts) {
    if (typeof part.path !== 'string' || !Number.isInteger(part.offset) || part.offset < 0) {
      throw new Error('A publicação do firmware está incompleta.');
    }
    const binaryUrl = new URL(part.path, manifestUrl);
    if (binaryUrl.origin !== manifestUrl.origin) throw new Error('O arquivo de firmware não pertence a esta publicação.');
    const binary = await fetch(binaryUrl.href, { method: 'HEAD', cache: 'no-store', signal });
    const length = binary.headers?.get('content-length');
    if (!binary.ok || (length !== null && length !== undefined && Number(length) <= 0)) {
      throw new Error('O arquivo de firmware não está disponível. Aguarde a publicação e verifique novamente.');
    }
  }
  return { version: manifest.version, manifestUrl: manifestUrl.href };
}

async function checkRelease() {
  if (checkingRelease) return;
  const button = document.getElementById('installButton');
  const installer = document.getElementById('installer');
  const status = document.getElementById('installStatus');
  const retry = document.getElementById('retryButton');
  const version = document.getElementById('version');
  button.disabled = true;
  installer.hidden = true;
  retry.hidden = true;
  retry.textContent = 'Verificar novamente';
  retry.onclick = checkRelease;
  version.textContent = 'Não verificada';
  if (!isSecureContext) {
    status.textContent = 'Abra esta página pelo endereço HTTPS para conectar o M5 por USB.';
    return;
  }
  if (!('serial' in navigator)) {
    status.textContent = 'Este navegador não oferece conexão USB serial. Abra esta página no Chrome ou Edge de um computador.';
    return;
  }
  checkingRelease = true;
  status.textContent = 'Verificando o firmware e carregando o instalador USB…';
  const controller = new AbortController();
  let timer;
  try {
    const readiness = Promise.all([
      publishedRelease(controller.signal),
      customElements.whenDefined('esp-web-install-button'),
    ]);
    const deadline = new Promise((resolve, reject) => {
      timer = setTimeout(() => {
        reject(new Error(customElements.get('esp-web-install-button')
          ? 'A verificação demorou demais. Confira sua conexão e tente novamente.'
          : 'Não foi possível carregar o instalador USB. Confira sua conexão e recarregue a página.'));
        controller.abort();
      }, 15000);
    });
    const [release] = await Promise.race([readiness, deadline]);
    installer.setAttribute('manifest', release.manifestUrl);
    version.textContent = release.version;
    status.textContent = 'Firmware disponível. Conecte o M5 por USB e abra o instalador.';
    installer.hidden = false;
    button.disabled = false;
  } catch (error) {
    status.textContent = error instanceof TypeError
      ? 'Não foi possível verificar a publicação. Confira sua conexão e tente novamente.'
      : error.message || 'Não foi possível verificar a publicação do firmware.';
    retry.hidden = false;
    if (!customElements.get('esp-web-install-button')) {
      retry.textContent = 'Recarregar página';
      retry.onclick = () => location.reload();
    }
  } finally {
    clearTimeout(timer);
    controller.abort();
    checkingRelease = false;
  }
}

checkRelease();
