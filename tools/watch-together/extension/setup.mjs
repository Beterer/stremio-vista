import { relayOrigin } from './adapter.mjs';

const input = document.querySelector('#relay');
const status = document.querySelector('#status');
const { relay = '' } = await chrome.storage.local.get('relay');
input.value = relay || input.defaultValue;

document.querySelector('form').addEventListener('submit', async event => {
  event.preventDefault();
  const origin = relayOrigin(input.value);
  if (!origin) { status.textContent = 'Enter the relay’s HTTPS origin.'; return; }
  const pattern = `${new URL(origin).protocol}//${new URL(origin).hostname}/*`;
  try {
    if (!await chrome.permissions.request({ origins: [pattern] })) {
      status.textContent = 'Allow access to the relay to join a room.';
      return;
    }
    await chrome.storage.local.set({ relay: origin });
    input.value = origin;
    status.textContent = 'Relay saved.';
  } catch {
    status.textContent = 'Could not save the relay address.';
  }
});
