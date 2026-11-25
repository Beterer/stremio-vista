import { relayOrigin } from './adapter.mjs';

chrome.runtime.onMessage.addListener((message, sender, respond) => {
  if (!sender.url?.startsWith('https://web.stremio.com/') || sender.frameId !== 0) return;
  if (message?.type === 'watch-options') {
    chrome.runtime.openOptionsPage();
    return;
  }
  if (message?.type !== 'watch-rpc') return;
  request(message).then(respond, () => respond({ error: 'unreachable' }));
  return true;
});

async function request({ origin, payload }) {
  const { relay } = await chrome.storage.local.get('relay');
  if (!relay || relayOrigin(relay) !== relay) return { error: 'configure' };
  if (relay !== origin) return { error: 'settings-changed' };
  if (payload?.op !== 'join' && payload?.op !== 'tick' && payload?.op !== 'leave') return { error: 'bad-request' };
  const body = JSON.stringify(payload);
  if (body.length > 1024) return { error: 'bad-request' };
  const response = await fetch(`${relay}/watch-together/v1`, {
    method: 'POST', headers: { 'content-type': 'application/json' }, body,
    cache: 'no-store', redirect: 'error', credentials: 'omit', signal: AbortSignal.timeout(2200),
  });
  if (!response.ok) return { error: 'unreachable' };
  const reader = response.body.getReader();
  const decoder = new TextDecoder();
  let result = '';
  while (true) {
    const { value, done } = await reader.read();
    if (done) return JSON.parse(result + decoder.decode());
    result += decoder.decode(value, { stream: true });
    if (result.length > 8192) { await reader.cancel(); return { error: 'bad-response' }; }
  }
}
