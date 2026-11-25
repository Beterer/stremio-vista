import test from 'node:test';
import assert from 'node:assert/strict';
import { deflateSync } from 'node:zlib';
import { detailsHash, isMediaRoute, matchingSource, playbackTarget, releaseKey } from '../../tools/watch-together/extension/adapter.mjs';

const media = { type: 'series', id: 'tt123', vid: 'tt123:1:2', hash: 'a'.repeat(40), file: 3,
  release: [releaseKey('Example.Episode.1080p.mkv')], duration: 2800 };
const route = (stream, video = media.vid) => '#/player/' +
  encodeURIComponent(deflateSync(JSON.stringify(stream)).toString('base64')) +
  '/streamAddon/metaAddon/series/tt123/' + encodeURIComponent(video);

test('opens the exact episode and validates the native Stremio player route', () => {
  assert.equal(detailsHash(media), '#/detail/series/tt123/tt123%3A1%3A2');
  assert.ok(isMediaRoute(route({}), 'player', media));
  assert.ok(!isMediaRoute(route({}, 'tt123:1:3'), 'player', media));
  assert.ok(isMediaRoute(detailsHash(media), 'details', media));
});

test('opens only a locally offered matching file, never another file in a season pack', async () => {
  const candidate = (stream, text = '', video) => ({ href: route(stream, video), text });
  const wrong = candidate({ infoHash: media.hash, fileIdx: 4 }, 'Example Episode 1080p');
  const right = candidate({ infoHash: media.hash.toUpperCase(), fileIdx: 3 });
  assert.equal(await matchingSource([wrong, right], media), right);
  assert.equal(await matchingSource([wrong], media), undefined);
  const debrid = candidate({ url: 'https://local.example/me-only', behaviorHints: { filename: 'Example.Episode.1080p.mkv' } });
  assert.equal(await matchingSource([debrid], media), debrid);
  assert.equal(await matchingSource([debrid, candidate({ url: 'https://other.example/path' }, 'Example Episode 1080p')], media), undefined);
  assert.equal(await matchingSource([candidate({ infoHash: media.hash, fileIdx: 3 }, '', 'tt123:1:3')], media), undefined);
});

test('both clients target the same position before the start and advance using relay time', () => {
  const room = { mode: 'playing', pos: 123.5, at: 5000, media };
  assert.equal(playbackTarget(room, 4000), 123.5);
  assert.equal(playbackTarget(room, 5500), 124);
  assert.equal(playbackTarget({ ...room, mode: 'paused' }, 9000), 123.5);
});
