<p align="center"><img src="resource/icon.png" width="96" alt="ThirdTape icon"></p>

<h1 align="center">ThirdTape</h1>

> **AI Disclaimer:** This project was developed with the assistance of Claude AI. I believe AI-written code should be open source to benefit everyone and maintain transparency.

A homebrew [Kamtape](https://www.kamtape.com) client for the Nintendo 3DS: browse, search and watch Kamtape videos with a native 3DS interface instead of the browser.

ThirdTape is an unofficial client and is not affiliated with Kamtape. It is based on [FourthTube](https://github.com/erievs/FourthTube) (a YouTube client): the interface, the video player and the streaming downloader come from there, and the YouTube data layer was replaced with one that talks to Kamtape.

> **Status:** work in progress. Developed in the Azahar emulator and confirmed working on a real New 3DS XL.

## Features
- **Home:** the videos featured on Kamtape's front page.
- **Search:** Kamtape's search with paging ("load more" as you scroll).
- **Video player:** streams Kamtape's MP4 files (`get_video?video_id=ID&webm=1`) with HTTP range requests, so playback starts before the whole file is downloaded and seeking works. Software decoding everywhere, plus the New 3DS hardware decoder inherited from FourthTube (confirmed in use on a New 3DS XL).
- **Ratings:** Kamtape's 5-star rating with the number of ratings. When logged in, tap a star under the title to rate the video (hold and slide to preview).
- **Comments:** with their scores (Reddit-style up/down column) and replies. The first comments load immediately, the rest as you scroll. When logged in you can post comments, reply to comments and vote comments up or down with the arrows next to the score.
- **Channels:** profile picture, bio and profile fields, counters, the user's videos, their playlists, and the comments left on the channel (with the commenters' pictures and attached videos). Posting a comment on a channel (Kamtape asks for a captcha) is implemented but currently bugged, so the button is switched off.
- **Subscriptions** and **watch history**. History is local to the app. When logged in, subscriptions are the account's: they are read from Kamtape at startup and after login, and subscribing/unsubscribing is sent to the account. (You can't subscribe to yourself.)
- **Login** with your Kamtape username and password (Settings). Only the session cookie is saved to the SD card, never the password. The session is sent with every request to kamtape.com.
- English only (like Kamtape).
- A UI in the style of the 3DS System Settings, with a green accent.

Known issues: posting on channels. Rating videos and voting on comments are new and have not been confirmed against the live site on a real console yet. Not supported yet: uploading, favorites, subtitles, live streams, shorts, community posts.

## Controls and data
Touch screen and buttons as in FourthTube (L/R switch tabs, B goes back). All of the app's data lives in `sdmc:/3ds/ThirdTape/` (watch history, subscriptions, settings, `log.txt`). It is separate from FourthTube's folder.

## Install
Copy `ThirdTape.cia` to the SD card and install it with a CIA installer such as FBI. A New 3DS gets better playback performance; an old 3DS works but is slow.

## Build
Requirements: [devkitPro](https://devkitpro.org/wiki/Getting_Started) with devkitARM and libctru, plus the portlibs `3ds-mbedtls` and `3ds-zlib`:

```
sudo dkp-pacman -S 3ds-mbedtls 3ds-zlib
```

The project path must not contain spaces. Then:

```
export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM
for d in $(cd source && find . -type d); do mkdir -p build/$d; done   # object directories (first build only)
make -j8
```

This produces `ThirdTape.3dsx` and `ThirdTape_dev.cia`. The CIA step uses `makerom` and `bannertool`; the copies in `resource/tools` are for Linux/Windows, so on macOS put native builds in `tools/bin` and run:

```
make -j8 MAKEROM=tools/bin/makerom BANNERTOOL=tools/bin/bannertool
```

The app descriptor (`resource/app.rsf`) lists the `mvd` system module as a dependency; that is what lets the New 3DS hardware video decoder start.

### Debugging
- `sdmc:/3ds/ThirdTape/log.txt` mirrors the in-app log (flushed per line, so it survives crashes).
- A file `sdmc:/3ds/ThirdTape/start_video.txt` containing a video URL (or `user:<name>`) opens it directly at launch, which is handy in an emulator.

## How it talks to Kamtape
See [`docs/KAMTAPE.md`](docs/KAMTAPE.md). In short: videos, details and user videos come from Kamtape's REST API; search, profiles, playlists and comment scores are read from its HTML pages. The data layer is `source/youtube_parser/kamtape.cpp` (the directory name is a leftover from FourthTube; the interface is unchanged so the rest of the app did not need to change).

## Credits and licence
- **FourthTube** by the FourthTube Community, a fork of **ThirdTube** by WindowsServer2003. The video decoder comes from **Video player for 3DS** by Core-2-Extreme. See [`UPSTREAM_README.md`](UPSTREAM_README.md) for the full credits.
- The Kamtape data layer, the Kamtape-specific interface changes and the green restyle were written with Claude AI.
- Icon and banner by RetroGigabyte.

Licensed under the GNU General Public License v3 (see [`LICENSE`](LICENSE)), like the projects it is derived from.
