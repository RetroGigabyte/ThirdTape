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
