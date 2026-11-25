const mediaId = /^[A-Za-z0-9_.:-]{1,79}$/;

export function detailsHash(media) {
  return '#/detail/' + [media.type, media.id, media.vid].map(encodeURIComponent).join('/');
}

function segments(hash) {
  try { return hash.split('?')[0].slice(2).split('/').map(decodeURIComponent); }
  catch { return []; }
}

export function validMedia(media) {
  return media && ['movie', 'series'].includes(media.type) &&
    typeof media.id === 'string' && mediaId.test(media.id) &&
    typeof media.vid === 'string' && mediaId.test(media.vid) &&
    Number.isFinite(media.duration) && media.duration >= 1 && media.duration <= 86400;
}

export function isMediaRoute(hash, kind, media) {
  const parts = segments(hash);
  const offset = kind === 'player' ? 4 : 1;
  const names = kind === 'player' ? ['player'] : ['detail', 'metadetails'];
  if (!names.includes(parts[0]) || parts[offset] !== media.type || parts[offset + 1] !== media.id) return false;
  return (parts[offset + 2] || (media.type === 'movie' ? media.id : '')) === media.vid;
}

export function releaseKey(line) {
  if (/[\\/:?&@]/.test(line)) return '';
  const clean = line.trim().replace(/\.(mkv|mp4|avi|mov|webm|m4v|ts)$/i, '').toLowerCase().replace(/[^a-z0-9]/g, '');
  if (clean.length < 8 || clean.length > 160) return '';
  let hash = 0xcbf29ce484222325n;
  for (const c of clean) hash = BigInt.asUintN(64, (hash ^ BigInt(c.charCodeAt(0))) * 0x100000001b3n);
  return hash.toString(16).padStart(16, '0');
}

async function streamFromHash(hash) {
  const encoded = segments(hash)[1];
  if (!encoded || encoded.length > 64_000) return null;
  try {
    const bytes = Uint8Array.from(atob(encoded), c => c.charCodeAt(0));
    const reader = new Blob([bytes]).stream().pipeThrough(new DecompressionStream('deflate')).getReader();
    const decoder = new TextDecoder();
    let output = '';
    while (true) {
      const { done, value } = await reader.read();
      if (done) return JSON.parse(output + decoder.decode());
      output += decoder.decode(value, { stream: true });
      if (output.length > 64_000) { await reader.cancel(); return null; }
    }
  } catch { return null; }
}

export async function matchingSource(links, media) {
  if (!media.hash && !media.videoHash && !media.release?.length) return;
  const matches = [];
  for (const candidate of links) {
    if (!isMediaRoute(candidate.href, 'player', media)) continue;
    const stream = await streamFromHash(candidate.href);
    if (!stream || stream.externalUrl || stream.playerFrameUrl || stream.ytId) continue;
    const hints = stream.behaviorHints ?? {};
    const hash = typeof stream.infoHash === 'string' ? stream.infoHash.toLowerCase() : '';
    if (media.hash && hash) {
      if (hash === media.hash && (stream.fileIdx ?? null) === (media.file ?? null)) return candidate;
      if (hash === media.hash) continue;
    }
    if (media.videoHash && typeof hints.videoHash === 'string') {
      if (media.videoHash === hints.videoHash.toLowerCase()) return candidate;
      continue;
    }
    const lines = [hints.filename, ...candidate.text.split(/\r?\n/)].filter(line => typeof line === 'string');
    if (lines.some(line => media.release?.includes(releaseKey(line)))) matches.push(candidate);
  }
  if (matches.length === 1) return matches[0];
}

export function playbackTarget(state, now) {
  const elapsed = state.mode === 'playing' ? Math.max(0, now - state.at) / 1000 : 0;
  return Math.min(state.pos + elapsed, state.media.duration - 0.1);
}

export function relayOrigin(value) {
  try {
    const url = new URL(value.trim());
    if (url.username || url.password || url.pathname !== '/' || url.search || url.hash) return '';
    if (url.protocol === 'https:' || (url.protocol === 'http:' && url.hostname === '127.0.0.1')) return url.origin;
  } catch { /* invalid address */ }
  return '';
}
