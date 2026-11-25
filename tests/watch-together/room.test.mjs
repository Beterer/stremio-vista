import test from 'node:test';
import assert from 'node:assert/strict';
import { createRelay } from '../../tools/watch-together/relay.mjs';

test('both players must stage the same position before a timed start', () => {
  let clock = 100_000;
  const rpc = createRelay(() => clock);
  const { code, token: tv, state: initial } = rpc({ op: 'create', media: {
    type: 'series', id: 'tt123', vid: 'tt123:1:2', duration: 2850, hash: 'a'.repeat(40), file: 3,
    release: ['0123456789abcdef'], url: 'https://private.example/token',
  }, pos: 42.5 });
  assert.equal(initial.pos, 42.5);
  const { token: web, state } = rpc({ op: 'join', code });
  assert.equal(state.media.url, undefined);
  assert.deepEqual(state.media.release, ['0123456789abcdef']);
  const tick = (token, more = {}) => rpc({ op: 'tick', token, ...more }).state;
  const ready = (token, rev = state.rev) => tick(token, { ready: rev });
  assert.equal(ready(tv).mode, 'prepare');
  assert.equal(ready(web, state.rev - 1).mode, 'prepare');
  const scheduled = ready(web);
  assert.equal(scheduled.mode, 'playing');
  assert.equal(scheduled.pos, 42.5);
  assert.ok(scheduled.at - scheduled.now >= 1000);
  assert.equal(tick(tv).at, scheduled.at);

  clock = scheduled.at + 10_000;
  const paused = tick(web, { action: 'pause', rev: scheduled.rev, pos: 52.4 });
  assert.equal(paused.mode, 'paused');
  assert.equal(tick(tv).pos, 52.4);
  assert.equal(tick(tv, { action: 'play', rev: scheduled.rev }).mode, 'paused');
  const resumed = tick(tv, { action: 'play', rev: paused.rev });
  assert.equal(resumed.mode, 'prepare');
  assert.equal(resumed.pos, 52.4);
  assert.equal(ready(web, resumed.rev).mode, 'prepare');
  assert.equal(ready(tv, resumed.rev).mode, 'playing');
  assert.equal(rpc({ op: 'leave', token: tv }).ok, true);
  assert.equal(rpc({ op: 'tick', token: web }).error, 'room-ended');
});

test('browser controls can seek, pause and resume without stale requests undoing a change', () => {
  const rpc = createRelay(() => 100_000);
  const { code, token: tv } = rpc({ op: 'create', media: { type: 'movie', id: 'tt123', vid: 'tt123', duration: 90 }, pos: 0 });
  const { token: web, state } = rpc({ op: 'join', code });
  const tick = (token, more) => rpc({ op: 'tick', token, ...more }).state;
  tick(tv, { ready: state.rev });
  const playing = tick(web, { ready: state.rev });
  const seek = { action: 'seek', rev: playing.rev, pos: 31.5 };
  const preparing = tick(web, seek);
  assert.equal(preparing.mode, 'prepare');
  assert.equal(preparing.pos, 31.5);
  assert.equal(tick(web, seek).rev, preparing.rev);
  assert.equal(tick(tv, { ready: playing.rev }).mode, 'prepare');
  tick(tv, { ready: preparing.rev });
  const afterSeek = tick(web, { ready: preparing.rev });
  assert.equal(afterSeek.mode, 'playing');
  const paused = tick(tv, { action: 'pause', rev: afterSeek.rev, pos: 31.5 });
  assert.equal(tick(tv, { action: 'seek', rev: paused.rev, pos: 200 }).pos, 89.9);
});

test('a disconnected browser releases its place and the host can start again with a replacement', () => {
  let clock = 100_000;
  const rpc = createRelay(() => clock);
  const { code, token: tv } = rpc({ op: 'create', media: { type: 'movie', id: 'tt123', vid: 'tt123', duration: 90 }, pos: 0 });
  const { token: web, state } = rpc({ op: 'join', code });
  assert.equal(rpc({ op: 'join', code }).error, 'room-full');
  rpc({ op: 'tick', token: tv, ready: state.rev });
  const playing = rpc({ op: 'tick', token: web, ready: state.rev }).state;
  clock += 10_000;
  rpc({ op: 'tick', token: tv });
  clock += 6000;
  const waiting = rpc({ op: 'tick', token: tv }).state;
  assert.equal(waiting.mode, 'prepare');
  assert.equal(waiting.pos, (clock - playing.at) / 1000);
  assert.equal(waiting.watchers, 1);
  assert.equal(rpc({ op: 'tick', token: web }).error, 'room-ended');
  assert.ok(rpc({ op: 'join', code }).token);
});

test('a ready announcement goes stale before a disconnected player can trigger a start', () => {
  let clock = 100_000;
  const rpc = createRelay(() => clock);
  const { code, token: tv, state } = rpc({ op: 'create', media: { type: 'movie', id: 'tt123', vid: 'tt123', duration: 90 }, pos: 0 });
  const { token: web } = rpc({ op: 'join', code });
  rpc({ op: 'tick', token: tv, ready: state.rev });
  clock += 4000;
  assert.equal(rpc({ op: 'tick', token: web, ready: state.rev }).state.mode, 'prepare');
  assert.equal(rpc({ op: 'tick', token: tv, ready: state.rev }).state.mode, 'playing');
});
