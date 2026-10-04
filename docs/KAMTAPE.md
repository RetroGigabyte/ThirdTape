# Kamtape notes (checked October 2026)

Kamtape is a 2006-style video site with server-rendered HTML pages and a small REST API.

## REST API (no developer key needed for the calls tried)
Base: `https://www.kamtape.com/api2_rest?method=<method>&...` - responses are XML (`<ut_response status="ok">`).
Documented methods (see `/dev` and `/dev_api_ref?m=<method>` on the site):
- `kamtape.videos.list_featured` - the most recent 25 front-page videos
- `kamtape.videos.get_details&video_id=ID` - details incl. comments, channels, tags, thumbnail
- `kamtape.videos.list_by_tag` (with paging), `kamtape.videos.list_by_user`
- `kamtape.users.get_profile`, `kamtape.users.list_favorite_videos`, `kamtape.users.list_friends`

Video entries contain: author, id, title, length_seconds, rating_avg, rating_count, description, view_count, upload_time
(unix), comment_count, tags, url, thumbnail_url (`/get_still?video_id=ID`).

## Video files
- `https://www.kamtape.com/get_video?video_id=ID` returns **FLV** (`video/x-flv`).
- `https://www.kamtape.com/get_video?video_id=ID&webm=1` returns **MP4** (`video/mp4`) despite the parameter name.
- Both answer `Accept-Ranges: bytes`, so progressive streaming and seeking are possible.
- The mobile site (`m.kamtape.com`) redirects to a `/warning` page first.

## Not covered by the API (HTML scraping needed)
Search, browse (`/browse?s=mp|mv|rf...`), categories (`/categories_portal?c=N`), channels/members pages.

## Pages that are scraped (no API equivalent)
- **Search:** `/results?search_type=search_videos&search_query=<q>&search_sort=relevance&search_category=0&page=<n>`; each
  result is a `vEntry` block (id, thumbnail, title, runtime, "Added", "Views"). A "Next" link means more pages.
- **Profile:** `/profile?user=<name>` (`/user/<name>` redirects there). The API's `about_me` is always empty; the bio is the
  unlabelled paragraph in the profile box (`id="pBox"`), the other fields are `<span class="smallText">Label: </span><b>value</b>`.
  The profile picture is the first image of that box (a 4:3 video thumbnail, `.../vi/<id>/2.jpg`).
- **Playlists:** `/profile_play_list?user=<name>` lists them (`/view_play_list?p=<id>`); `/view_play_list?p=<id>` lists the videos.
  A video in a playlist is opened as `/watch?v=<id>&list=<playlist id>` in the app.
- **Comments:** the watch page has the first ones; `/comment_servlet?all_comments&v=<id>&fromurl=/watch?v=<id>` has all of
  them. Each comment (`commentEntry`, replies are `commentEntryReply`) has a score in `comment_score_<comment id>` ("+9", "0", ...).
- **Video files:** `get_video?video_id=<id>&webm=1` is MP4 with `Accept-Ranges: bytes` (the index is at the start of the
  file). Range requests must use the `Range` header; Kamtape ignores YouTube-style `&range=` parameters and answers with the whole file.
