# ThirdTape change list

ThirdTape started as an unmodified copy of [FourthTube](https://github.com/erievs/FourthTube) (a YouTube client). Everything
below is what changed since then. Version numbers are not used yet; entries are grouped by theme, newest features last.

## Kamtape instead of YouTube
- The YouTube data layer was replaced by one for Kamtape: featured videos (home), search with paging, video details,
  related videos and user ("channel") pages. Videos use Kamtape's MP4 stream (`get_video?video_id=ID&webm=1`).
- Streaming and seeking work: the downloader sends standard `Range` headers (Kamtape ignores YouTube's `&range=`).
- The hardware video decoder is probed with a timeout, so emulators fall back to software decoding instead of hanging.
- The single combined MP4 is offered as the 360p quality, and audio-only mode plays the same file.
- Numeric HTML entities in text (for example `&#039;`) are decoded.

## Video page
- 5-star rating (Kamtape's own star images, partial stars) and the number of ratings, instead of thumbs up/down.
- Comments with their scores in a Reddit-style up/down column, and nested replies. The first comments load immediately, the
  rest when you scroll.
- Subtitles tab and the 3D option removed (Kamtape has neither).
- Seek bar, its pointer and the buffering pointer are dark green.
- Playlists: opening one plays through it, with the playlist tab showing its videos.

## Channels
- Profile picture, bio and profile fields (name, age, country, interests, ...), video count and subscriber count.
- Tabs: Videos, Playlists, Comments, Info (Live, Shorts and Posts removed).
- Playlists tab loads the user's playlists.
- Comments tab lists the comments left on the channel, 10 per page, with the commenters' pictures and attached videos.
- No Subscribe button on your own channel or videos.

## Account
- Login with Kamtape username and password (Settings). Only the session cookie is stored, never the password; it stays
  after a restart. Log out removes it.
- "My channel" entry in the hamburger menu for logged-in users.
- Subscriptions follow the account: read at startup and after login; subscribing and unsubscribing are sent to Kamtape.
- Commenting on videos and replying to comments (Settings login required).
- Posting on channels is implemented but switched off ("Posting is bugged (for now)"); see Known issues.

## Look and feel
- Restyled after the 3DS System Settings: blue-grey/green palette with a green accent, flat tab bars with an underline on the
  selected tab, rounded bordered buttons and selectors, light-green press highlight, bordered thumbnails and thin row separators.
- The wide (800x240) top screen is turned off: the picture was stretched and shifted because the drawing code assumes 400 px.
- New icon and banner (`resource/icon.png`, `resource/banner_legacy.png`).
- English only (language selectors removed). Settings: Update tab and "App data to use" removed; no update check.

## Under the hood
- Own data folder `sdmc:/3ds/ThirdTape/` (history, subscriptions, settings, account, log), separate from FourthTube.
- The in-app log is mirrored to `log.txt`; a file `start_video.txt` (a video URL or `user:<name>`) opens it at launch (debugging).
- The HTTP layer keeps every `Set-Cookie` header and sends the session cookie to kamtape.com requests.
- CIA descriptor with the `mvd` dependency (hardware video decoding on a New 3DS), one CIA for all models.
- README, credits and `docs/KAMTAPE.md` (how each Kamtape page and endpoint is used).

## Known issues
- Posting a comment on a channel (needs a captcha) does not work yet.
- Confirmed working on a New 3DS XL. Not tested on an old 3DS/2DS, which has no hardware video decoder (software decoding only).
- Voting on comments, rating videos, favorites and uploading are not implemented.

## Rating and voting
- Rate a video: while logged in, tap one of the five stars under the title (hold and slide to preview). The rating count and
  average update afterwards.
- Vote on comments and replies: while logged in, the up and down arrows next to a comment's score are tappable. The score
  changes at once and goes back if the vote fails.
- Not yet confirmed against the live site on a real console.
