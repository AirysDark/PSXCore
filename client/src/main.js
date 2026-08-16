import './style.css';

const state = {
  connected: false,
  controller: 'NONE',
  psx: 'WAIT',
  ble: 'ADVERTISING',
  battery: '--',
  pins: { data: 4, cmd: 5, att: 6, clk: 7, ack: 8 },
  tab: 'Console',
  log: ['PSXCore Control client ready.', 'Waiting for PSXCore device connection.']
};

const tabs = ['Console', 'Controller', 'Pins', 'Settings', 'Firmware'];

function render() {
  document.querySelector('#app').innerHTML = `
    <header class="topbar">
      <div><strong>PSXCore</strong><span> CONTROL</span></div>
      <div class="connection ${state.connected ? 'online' : ''}">${state.connected ? 'CONNECTED' : 'NOT CONNECTED'}</div>
    </header>
    <main>
      <nav class="tabs">${tabs.map(t => `<button class="tab ${state.tab === t ? 'active' : ''}" data-tab="${t}">${t}</button>`).join('')}</nav>
      ${state.tab === 'Console' ? consoleView() : ''}
      ${state.tab === 'Controller' ? controllerView() : ''}
      ${state.tab === 'Pins' ? pinsView() : ''}
      ${state.tab === 'Settings' ? settingsView() : ''}
      ${state.tab === 'Firmware' ? firmwareView() : ''}
    </main>
  `;
  document.querySelectorAll('[data-tab]').forEach(b => b.onclick = () => { state.tab = b.dataset.tab; render(); });
  document.querySelectorAll('[data-action]').forEach(b => b.onclick = () => action(b.dataset.action));
}

function statusCards() {
  return `<section class="grid">
    <div class="card"><label>Controller</label><b>${state.controller}</b></div>
    <div class="card"><label>PSX Bus</label><b>${state.psx}</b></div>
    <div class="card"><label>BLE</label><b>${state.ble}</b></div>
    <div class="card"><label>Battery</label><b>${state.battery}${state.battery === '--' ? '' : '%'}</b></div>
  </section>`;
}

function consoleView() {
  return `${statusCards()}<section class="panel"><div class="panel-head"><h2>Console</h2><button data-action="clear">Clear</button></div><pre>${state.log.join('\n')}</pre><div class="actions"><button data-action="connect">Connect</button><button data-action="refresh">Refresh status</button><button data-action="sweep">Run pin sweep</button></div></section>`;
}

function controllerView() {
  return `<section class="panel"><h2>Controller</h2><div class="controller-box"><div class="cross">D-PAD</div><div class="face">△ ○ × □</div></div><div class="grid compact"><div class="card"><label>Type</label><b>${state.controller}</b></div><div class="card"><label>Left stick</label><b>128 / 128</b></div><div class="card"><label>Right stick</label><b>128 / 128</b></div><div class="card"><label>Battery</label><b>${state.battery}</b></div></div></section>`;
}

function pinsView() {
  return `<section class="panel"><h2>PSX Pin Mapping</h2><p class="muted">Current firmware mapping</p><div class="pin-grid">${Object.entries(state.pins).map(([k,v]) => `<label>${k.toUpperCase()}<input type="number" value="${v}" data-pin="${k}" /></label>`).join('')}</div><div class="actions"><button data-action="savepins">Save pins</button><button data-action="sweep">Run pin sweep</button></div></section>`;
}

function settingsView() {
  return `<section class="panel"><h2>Settings</h2><label class="setting">Device name<input value="PSXCore" /></label><label class="setting">Status interval (ms)<input type="number" value="1000" /></label><label class="setting">Auto pin sweep<select><option>On controller failure</option><option>Always on boot</option><option>Disabled</option></select></label><label class="setting"><input type="checkbox" checked /> Enable runtime diagnostics</label><div class="actions"><button data-action="save">Save settings</button><button data-action="defaults">Restore defaults</button></div></section>`;
}

function firmwareView() {
  return `<section class="panel"><h2>Firmware</h2><div class="firmware"><p>Installed: <b>PSXCore</b></p><p>Version: <b>Unknown</b></p><p>Partition: <b>OTA</b></p><p>PSRAM: <b>Detected by device</b></p></div><div class="actions"><button data-action="checkfw">Check firmware</button><button data-action="updatefw">Start update</button></div><p class="muted">Firmware flashing is intentionally disabled until a device transport is connected.</p></section>`;
}

function action(a) {
  if (a === 'connect') { state.connected = !state.connected; state.log.push(state.connected ? '[CLIENT] Device connected.' : '[CLIENT] Device disconnected.'); }
  if (a === 'refresh') state.log.push('[CLIENT] Status refresh requested.');
  if (a === 'sweep') state.log.push('[CLIENT] Pin sweep requested: GPIO 4,5,6,7,8.');
  if (a === 'savepins') state.log.push('[CLIENT] Pin mapping save requested.');
  if (a === 'save') state.log.push('[CLIENT] Settings save requested.');
  if (a === 'defaults') state.log.push('[CLIENT] Default settings requested.');
  if (a === 'checkfw') state.log.push('[CLIENT] Firmware check requested.');
  if (a === 'updatefw') state.log.push('[CLIENT] Firmware update requested.');
  if (a === 'clear') state.log = [];
  render();
}

render();
