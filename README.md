# ThirdTape
> **AI Disclaimer:** This project was developed with the assistance of Claude AI. I believe AI-written code should be open source to benefit everyone and maintain transparency.
A homebrew [Kamtape](https://www.kamtape.com) client for the Nintendo 3DS.

> **Status: work in progress.** This repository currently contains an unmodified snapshot of
> [FourthTube](https://github.com/erievs/FourthTube) (a YouTube client). The plan is to keep its UI, hardware-accelerated
> player and streaming downloader, and replace the YouTube data layer (`source/youtube_parser/`) with one that talks to Kamtape.
> Until that is done, the app still behaves like FourthTube.

## Plan
1. Get the unmodified project building (needs the devkitPro packages `3ds-mbedtls` and `3ds-zlib`).
2. Write a Kamtape data layer behind the `parser.hpp` interface: featured/recent videos, search, video details, comments, users.
3. Remove the YouTube-only features (OAuth, shorts, live, ReturnYouTubeDislike, ...) and rebrand.
4. Build a CIA (the app descriptor must list the `mvd` module as a dependency for hardware video decoding).

See `docs/KAMTAPE.md` for what is known about Kamtape's API and video URLs.

## Credits and licence
Based on **FourthTube** (the FourthTube Community), which is a fork of **ThirdTube** by WindowsServer2003, whose video decoder
comes from **Video player for 3DS** by Core-2-Extreme. See `UPSTREAM_README.md` for the full credits. Licensed under the
GNU General Public License v3 (see `LICENSE`), like the projects it is derived from.
