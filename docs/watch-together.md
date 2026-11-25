# Watch Together

Host from Stremio Vista on the TV and invite one person using Stremio Web in Chrome or Edge. Once both players are ready, they start from the TV’s room position automatically. Either person can pause, resume, or seek.

## Set up once

1. Run the relay on a computer or server with Node.js 24 or later. Use an HTTPS address that both the TV and your friend can reach.

   ```sh
   node tools/watch-together/relay.mjs
   ```

   The relay listens on `127.0.0.1:8744` by default. On a server with Caddy, use this configuration with your own domain.

   ```caddyfile
   watch.example.com {
       reverse_proxy 127.0.0.1:8744
   }
   ```

   `GET /health` returns `{"ok":true}`. `WATCH_HOST` and `WATCH_PORT` can override the listen address and port. Keep the service running while watching. HTTP on `127.0.0.1` is available for local testing.

2. On the TV, go to **Settings → Playback → Watch Together relay**. Open the QR code on a phone connected to the TV’s network. Save the relay address, for example `https://watch.example.com`.
3. On your friend’s computer, extract the Chrome setup ZIP from the [Vista release](https://github.com/Beterer/stremio-vista/releases/latest). Open `chrome://extensions` or `edge://extensions`, turn on **Developer mode**, and choose **Load unpacked**. Select the extracted `extension` folder containing `manifest.json`. From a source checkout, select `tools/watch-together/extension`. Open the extension’s settings, enter the same relay address, and allow access to that address.

## Watch a movie or episode

1. Open a source in Vista and bring up the player controls. Select **Watch Together** to create the room at the TV’s current playback position. Share the room code shown on the TV.
2. Your friend opens **Watch Together** on `web.stremio.com`, enters the code, and chooses **Join**. The extension opens the movie or episode and looks for a matching source from their own addons.
3. Both players seek and buffer before the room starts playback. If the extension can’t identify a match, your friend can select the same release on the Stremio stream list and the room handles the rest. If Chrome or Edge blocks autoplay, the panel shows **Allow playback**.

Use the same cut on both devices and the built-in player in Stremio Web. The room can align timestamps across encodes of that cut. The browser checks duration to catch some mismatches, such as an extended cut. If playback drifts after buffering, the players seek back toward the room timeline.

Some titles with custom addon IDs are unsupported.

To end the room, select **End watch room** or exit the TV player. A room is for one movie or episode. Opening another video in Vista ends the room.

The relay keeps room codes and playback state in memory. It receives media IDs, duration, and release fingerprints to help the browser match sources. Each player gets its video directly from its own source.
