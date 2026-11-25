import { randomBytes } from 'node:crypto';
import { createServer } from 'node:http';
import { pathToFileURL } from 'node:url';

const alphabet = '23456789ABCDEFGHJKLMNPQRSTUVWXYZ';
const idleMs = 15_000;
const startLeadMs = 1500;
const readyFreshMs = 1500;
const mediaId = /^[A-Za-z0-9_.:-]{1,79}$/;
const tokenPattern = /^[a-f0-9]{48}$/;

function pickMedia(value) {
  if (!value || !['movie', 'series'].includes(value.type) ||
      typeof value.id !== 'string' || !mediaId.test(value.id) ||
      typeof value.vid !== 'string' || !mediaId.test(value.vid) ||
      !Number.isFinite(value.duration) || value.duration < 1 || value.duration > 86400) return null;
  const { type, id, vid, duration } = value;
  const media = { type, id, vid, duration };
  if (typeof value.hash === 'string' && /^[a-f\d]{40}$/i.test(value.hash)) media.hash = value.hash.toLowerCase();
  if (typeof value.videoHash === 'string' && /^[a-f\d]{16}$/i.test(value.videoHash)) media.videoHash = value.videoHash.toLowerCase();
  if (Number.isSafeInteger(value.file) && value.file >= 0 && value.file <= 65535) media.file = value.file;
  if (Array.isArray(value.release)) {
    media.release = value.release.filter(name => typeof name === 'string' && /^[a-f0-9]{16}$/.test(name)).slice(0, 4);
  }
  return media;
}

export function createRelay(clock = Date.now) {
  const rooms = new Map();
  const members = new Map();
  let nextSweep = 0;

  const bounded = (room, pos) => Math.min(Math.max(0, pos), room.media.duration - 0.1);
  const position = (room, now) => room.mode === 'playing'
    ? bounded(room, room.pos + Math.max(0, now - room.at) / 1000) : room.pos;
  function transition(room, mode, pos, at = 0) {
    Object.assign(room, { mode, pos, at, rev: room.rev + 1 });
  }
  function remove(room, member, now) {
    members.delete(member.token);
    if (member === room.host) {
      if (room.guest) members.delete(room.guest.token);
      rooms.delete(room.code);
    } else {
      room.guest = null;
      transition(room, room.mode === 'paused' ? 'paused' : 'prepare', position(room, now));
    }
  }
  function expire(room, now) {
    if (now - room.host.seen > idleMs) remove(room, room.host, now);
    else if (room.guest && now - room.guest.seen > idleMs) remove(room, room.guest, now);
  }
  function memberFor(room, now, client) {
    const member = { room, token: randomBytes(24).toString('hex'), seen: now, ready: 0, client };
    members.set(member.token, member);
    return member;
  }
  function snapshot(room, now) {
    const { rev, media, mode, pos, at } = room;
    return { rev, media, mode, pos, at, now, watchers: room.guest ? 2 : 1 };
  }
  function control(room, req, now) {
    if (req.rev !== room.rev) return;
    const current = position(room, now);
    if (req.action === 'pause' && room.mode !== 'paused') {
      const reported = Number.isFinite(req.pos) && Math.abs(req.pos - current) < 3 ? req.pos : current;
      transition(room, 'paused', bounded(room, reported));
    } else if (req.action === 'play' && room.mode === 'paused') {
      transition(room, 'prepare', current);
    } else if (req.action === 'seek' && Number.isFinite(req.pos)) {
      transition(room, room.mode === 'paused' ? 'paused' : 'prepare', bounded(room, req.pos));
    } else if (req.action === 'restart') {
      transition(room, 'prepare', 0);
    }
  }

  return req => {
    const now = clock();
    if (now >= nextSweep) {
      for (const room of rooms.values()) expire(room, now);
      nextSweep = now + 1000;
    }
    if (!req || typeof req !== 'object') return { error: 'bad-request' };
    if (req.op === 'create') {
      const media = pickMedia(req.media);
      if (!media || !Number.isFinite(req.pos)) return { error: 'bad-media' };
      if (rooms.size >= 128) return { error: 'relay-full' };
      let code;
      do { code = Array.from(randomBytes(10), value => alphabet[value & 31]).join(''); } while (rooms.has(code));
      const room = { code, media, rev: 1, mode: 'prepare', pos: bounded({ media }, req.pos), at: 0, guest: null };
      room.host = memberFor(room, now);
      rooms.set(code, room);
      return { code, token: room.host.token, state: snapshot(room, now) };
    }
    if (req.op === 'join') {
      const code = typeof req.code === 'string' ? req.code.trim().toUpperCase() : '';
      const room = rooms.get(code);
      if (room) expire(room, now);
      if (!rooms.has(code)) return { error: 'room-ended' };
      const client = typeof req.client === 'string' && tokenPattern.test(req.client) ? req.client : null;
      if (room.guest && (!client || room.guest.client !== client)) return { error: 'room-full' };
      room.guest ??= memberFor(room, now, client);
      room.guest.seen = now;
      return { token: room.guest.token, state: snapshot(room, now) };
    }

    const member = members.get(req.token);
    if (member) expire(member.room, now);
    if (!members.has(req.token)) return { error: 'room-ended' };
    const { room } = member;
    if (req.op === 'leave') {
      remove(room, member, now);
      return { ok: true };
    }
    if (req.op !== 'tick') return { error: 'bad-request' };
    member.seen = now;
    control(room, req, now);
    member.ready = req.ready === room.rev ? room.rev : 0;
    if (room.mode === 'prepare' && room.host.ready === room.rev && room.guest?.ready === room.rev &&
        now - room.host.seen <= readyFreshMs && now - room.guest.seen <= readyFreshMs) {
      transition(room, 'playing', room.pos, now + startLeadMs);
    }
    return { state: snapshot(room, now) };
  };
}

export function createWatchServer(rpc = createRelay()) {
  return createServer({ requestTimeout: 5000, headersTimeout: 5000 }, async (req, res) => {
    function send(status, json) {
      res.writeHead(status, { 'content-type': 'application/json', 'cache-control': 'no-store' });
      res.end(JSON.stringify(json));
    }
    if (req.method === 'GET' && req.url === '/health') return send(200, { ok: true });
    if (req.method !== 'POST' || req.url !== '/watch-together/v1') return send(404, { error: 'not-found' });
    if (req.headers['content-type']?.split(';')[0] !== 'application/json') return send(415, { error: 'bad-request' });
    const chunks = [];
    let length = 0;
    try {
      for await (const chunk of req.iterator({ destroyOnReturn: false })) {
        length += chunk.length;
        if (length > 2048) {
          res.setHeader('connection', 'close');
          return send(413, { error: 'bad-request' });
        }
        chunks.push(chunk);
      }
      return send(200, rpc(JSON.parse(Buffer.concat(chunks).toString('utf8'))));
    } catch {
      if (!res.headersSent && !res.destroyed) send(400, { error: 'bad-request' });
    }
  });
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const port = Number(process.env.WATCH_PORT ?? 8744);
  const host = process.env.WATCH_HOST || '127.0.0.1';
  createWatchServer().listen(port, host, () => console.log(`Vista Watch Together relay on ${host}:${port}`));
}
