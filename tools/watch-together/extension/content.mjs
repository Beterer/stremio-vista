import { detailsHash, isMediaRoute, matchingSource, playbackTarget, validMedia } from './adapter.mjs';

const host = document.createElement('aside');
const ui = host.attachShadow({ mode: 'closed' });
ui.innerHTML = `
  <style>
    :host { position: fixed; z-index: 2147483646; top: 14px; right: 14px; color-scheme: dark; font: 14px/1.4 system-ui, sans-serif; color: #f5f1ff; }
    [hidden] { display: none !important; }
    button, input { box-sizing: border-box; border: 1px solid #655b7d; border-radius: 8px; background: #252133; padding: 8px 10px; color: inherit; font: inherit; }
    button { cursor: pointer; font-weight: 620; }
    button:hover, button:focus-visible { border-color: #ded0ff; }
    #toggle { float: right; }
    #panel { clear: both; width: min(278px, 85vw); padding: 14px; border: 1px solid #655b7d; border-radius: 12px; background: #171521; box-shadow: 0 12px 40px #0008; }
    form { display: flex; gap: 6px; margin-top: 6px; }
    input { min-width: 0; flex: 1; text-transform: uppercase; letter-spacing: .08em; }
    #join { background: #ded0ff; color: #20143b; }
    p { margin: 9px 0; }
    #settings { background: transparent; }
    #enable { margin: 0 6px 6px 0; }
  </style>
  <button id="toggle" aria-controls="panel" aria-expanded="false">Watch Together</button>
  <section id="panel" aria-label="Watch Together" hidden>
    <label id="label" for="code">Room code</label>
    <form><input id="code" autocomplete="off" maxlength="12" required><button id="join">Join</button></form>
    <p id="status" role="status"></p>
    <button id="enable" hidden>Allow playback</button>
    <button id="leave" hidden>Leave room</button>
    <button id="settings">Relay settings</button>
  </section>`;
document.documentElement.append(host);

const get = selector => ui.querySelector(selector);
const panel = get('#panel');
const toggle = get('#toggle');
const status = get('#status');
const allow = get('#enable');
const form = get('form');
let room;
let joining = false;
let joinAttempt;

function show(open) {
  panel.hidden = !open;
  toggle.setAttribute('aria-expanded', String(open));
}

function say(text) {
  if (status.textContent !== text) status.textContent = text;
}

function showRoom(active) {
  form.hidden = get('#label').hidden = get('#settings').hidden = active;
  get('#leave').hidden = !active;
  if (!active) allow.hidden = true;
  toggle.textContent = active ? 'Watch Together •' : 'Watch Together';
}

function rpc(origin, payload) {
  return chrome.runtime.sendMessage({ type: 'watch-rpc', origin, payload });
}

function validState(state) {
  return state && validMedia(state.media) && ['prepare', 'playing', 'paused'].includes(state.mode) &&
    Number.isSafeInteger(state.rev) && state.rev > 0 && Number.isFinite(state.pos) &&
    state.pos >= 0 && state.pos < state.media.duration && Number.isFinite(state.at) && Number.isFinite(state.now);
}

function readState(session, state, sent) {
  const now = performance.now();
  const rtt = now - sent;
  if (!session.clockAt || now - session.clockAt > 20_000 || rtt < session.rtt) {
    session.rtt = rtt;
    session.clockAt = now;
    session.offset = state.now - (sent + now) / 2;
  }
  session.lastReply = now;
  session.state = state;
}

function quit(message) {
  const session = room;
  room = undefined;
  if (session) {
    session.events?.abort();
    rpc(session.origin, { op: 'leave', token: session.token }).catch(() => {});
  }
  showRoom(false);
  say(message);
}

async function join(event) {
  event.preventDefault();
  if (room || joining) return;
  const code = get('#code').value.trim().toUpperCase();
  if (!/^[2-9A-HJ-NP-Z]{10}$/.test(code)) { say('Enter the ten-character code shown on the TV.'); return; }
  joining = true;
  get('#join').disabled = true;
  say('Joining…');
  try {
    const { relay } = await chrome.storage.local.get('relay');
    if (!relay) { say('Add your relay address in Relay settings.'); return; }
    if (joinAttempt?.relay !== relay || joinAttempt.code !== code) {
      const client = Array.from(crypto.getRandomValues(new Uint8Array(24)), b => b.toString(16).padStart(2, '0')).join('');
      joinAttempt = { relay, code, client };
    }
    const sent = performance.now();
    const result = await rpc(relay, { op: 'join', code, client: joinAttempt.client });
    if (!result || !validState(result.state) || !/^[a-f0-9]{48}$/.test(result.token)) {
      say(result?.error === 'room-full' ? 'Someone has already joined this room.' :
        result?.error === 'room-ended' ? 'That room is closed or the code is wrong.' : 'Could not reach the relay.');
      return;
    }
    room = { token: result.token, origin: relay, nextPoll: 0, nextScan: 0, nextSeek: 0, nextPlay: 0, started: false };
    joinAttempt = undefined;
    readState(room, result.state, sent);
    showRoom(true);
    say('Finding the matching stream…');
    if (!isMediaRoute(location.hash, 'player', room.state.media) && !isMediaRoute(location.hash, 'details', room.state.media)) {
      location.hash = detailsHash(room.state.media);
    }
  } catch { say('Could not reach the relay.'); }
  finally { joining = false; get('#join').disabled = false; }
}

function stageSource(session, now) {
  if (!isMediaRoute(location.hash, 'details', session.state.media)) return;
  if (now < session.nextScan || session.scanning || session.openedSource) return;
  session.scanning = true;
  const route = location.hash;
  const links = new Map();
  for (const element of document.querySelectorAll('a[href^="#/player/"], a[href*="web.stremio.com/#/player/"]')) {
    if (links.size >= 100 || element.target === '_blank' || element.hasAttribute('download')) continue;
    const url = new URL(element.href);
    if (url.origin !== location.origin || links.has(url.hash)) continue;
    const text = [element.innerText, ...Array.from(element.querySelectorAll('[title]'), node => node.title)].join('\n');
    links.set(url.hash, { href: url.hash, text });
  }
  matchingSource(links.values(), session.state.media).then(match => {
    if (room !== session || location.hash !== route) return;
    if (match) {
      session.openedSource = true;
      say('Loading the matching stream…');
      location.hash = match.href;
    } else if (links.size) say('Choose the same release if a match does not open automatically.');
  }).finally(() => { session.scanning = false; session.nextScan = performance.now() + 1100; });
}

function queue(session, action, pos) {
  session.pending = { action, pos, rev: session.state.rev };
  session.nextPoll = 0;
}

function pauseLocal(session) {
  if (session.video.paused) return;
  session.syntheticPause = performance.now() + 800;
  session.video.pause();
}

function setVideo(session, video) {
  session.events?.abort();
  session.video = video;
  session.started = false;
  session.attachedAt = performance.now();
  session.nextSeek = 0;
  session.nextPlay = 0;
  session.userSeek = 0;
  session.syntheticSeek = undefined;
  session.syntheticPlay = session.syntheticPause = 0;
  session.blocked = false;
  allow.hidden = true;
  if (!video) return;
  session.events = new AbortController();
  const options = { signal: session.events.signal };
  video.addEventListener('play', () => {
    if (performance.now() < session.syntheticPlay) return;
    const { state } = session;
    if (state.mode === 'paused') queue(session, 'play', video.currentTime);
    if (state.mode !== 'playing' || performance.now() + session.offset < state.at) pauseLocal(session);
  }, options);
  video.addEventListener('pause', () => {
    if (session.started && !video.ended && session.state.mode === 'playing' &&
        performance.now() >= session.syntheticPause) queue(session, 'pause', video.currentTime);
  }, options);
  video.addEventListener('seeking', () => {
    if (!session.started && session.state.mode !== 'paused') return;
    if (!session.syntheticSeek || Math.abs(session.syntheticSeek.pos - video.currentTime) > 0.35 ||
        performance.now() > session.syntheticSeek.until) session.userSeek = performance.now() + 3500;
  }, options);
  video.addEventListener('seeked', () => {
    if (session.userSeek) { session.userSeek = 0; queue(session, 'seek', video.currentTime); }
    session.syntheticSeek = undefined;
  }, options);
}

function align(session, target, tolerance, now) {
  const video = session.video;
  if (video.seeking || video.readyState < 1 || !Number.isFinite(video.currentTime)) return false;
  if (Math.abs(video.currentTime - target) <= tolerance) return true;
  if (now >= session.nextSeek) {
    session.nextSeek = now + 1300;
    session.syntheticSeek = { pos: target, until: now + 3000 };
    video.currentTime = target;
  }
  return false;
}

function playLocal(session, now) {
  const video = session.video;
  if (!video || !video.paused || now < session.nextPlay) return;
  session.nextPlay = now + 900;
  session.syntheticPlay = now + 800;
  video.play().then(() => {
    if (room === session && session.video === video) { session.started = true; session.blocked = false; allow.hidden = true; }
  }, error => {
    if (room !== session || session.video !== video || error.name !== 'NotAllowedError') return;
    session.blocked = true;
    allow.hidden = false;
    show(true);
    if (session.state.mode === 'playing') queue(session, 'pause', video.currentTime);
  });
}

function adjust(session, now) {
  const { state } = session;
  const isPlayer = isMediaRoute(location.hash, 'player', state.media);
  const video = isPlayer ? document.querySelector('video') : null;
  if (video !== session.video) setVideo(session, video);
  if (!video) { stageSource(session, now); return 0; }
  if (video.error) { say('This source could not play. Go back and choose the same release.'); return 0; }
  if (Number.isFinite(video.duration) && Math.abs(video.duration - state.media.duration) > 2) {
    pauseLocal(session);
    say('This release has a different runtime. Go back and choose the same cut.');
    return 0;
  }
  if (video.ended) return 0;
  if (session.userSeek > now || session.pending) return 0;
  session.userSeek = 0;
  const relayNow = now + session.offset;
  const elapsed = relayNow - state.at;
  if (state.mode !== 'playing' || elapsed < 0) {
    pauseLocal(session);
    const aligned = align(session, state.pos, 0.22, now);
    const ready = aligned && Number.isFinite(video.duration) && video.readyState >= 3 && !session.blocked;
    const seekable = !video.seekable.length || Array.from({ length: video.seekable.length }, (_, i) => i)
      .some(i => state.pos >= video.seekable.start(i) - 0.22 && state.pos <= video.seekable.end(i) + 0.22);
    const chooseSource = now - session.attachedAt > 12_000 && !aligned && !seekable;
    say(session.blocked ? 'Your browser is waiting for permission to play.' : chooseSource ? 'This source cannot seek to the room position yet. Try another copy of the same cut.' :
      state.mode === 'paused' ? 'Room paused' : state.mode === 'playing' ? 'Starting together…' : ready ? 'Ready. Waiting for the TV…' : 'Buffering at the room’s position…');
    return ready && state.mode === 'prepare' ? state.rev : 0;
  }
  if (session.blocked) return 0;
  const target = playbackTarget(state, relayNow);
  if (video.paused) {
    if (!video.seeking && (elapsed < 500 || align(session, target, 0.85, now))) playLocal(session, now);
  } else {
    session.started = true;
    if (elapsed > 1800 && video.readyState >= 3) align(session, target, 1.1, now);
  }
  say(video.readyState >= 3 ? 'Watching together' : 'Buffering…');
  return 0;
}

async function poll(session, ready, now) {
  const pending = session.pending;
  session.inflight = true;
  session.nextPoll = now + 300;
  try {
    const result = await rpc(session.origin, { op: 'tick', token: session.token, ready, ...pending });
    if (room !== session) return;
    if (result?.error === 'room-ended' || result?.error === 'settings-changed') { quit('The Watch Together room ended.'); return; }
    if (validState(result?.state)) {
      readState(session, result.state, now);
      if (pending === session.pending) session.pending = undefined;
    }
  } catch { /* the next poll retries */ }
  finally { session.inflight = false; }
}

setInterval(() => {
  const session = room;
  if (!session) return;
  const now = performance.now();
  if (now - session.lastReply > 10_000) { quit('Lost the connection to the Watch Together room.'); return; }
  const ready = adjust(session, now);
  if (!session.inflight && (now >= session.nextPoll || ready !== session.lastReady)) {
    session.lastReady = ready;
    poll(session, ready, now);
  }
}, 50);

toggle.addEventListener('click', () => show(panel.hidden));
get('#settings').addEventListener('click', () => chrome.runtime.sendMessage({ type: 'watch-options' }));
get('#leave').addEventListener('click', () => quit('You left the room.'));
form.addEventListener('submit', join);
allow.addEventListener('click', () => {
  if (!room?.video) return;
  const session = room;
  const video = session.video;
  session.syntheticPlay = performance.now() + 800;
  video.play().then(() => {
    if (room !== session || session.video !== video) return;
    pauseLocal(session);
    session.blocked = false;
    session.nextPlay = 0;
    allow.hidden = true;
    if (session.state.mode === 'paused') queue(session, 'play', session.state.pos);
  }, () => say('Your browser is waiting for permission to play.'));
});
