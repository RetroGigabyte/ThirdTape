// Kamtape data layer (replaces the former YouTube parser behind parser.hpp).
// Uses Kamtape's REST API (https://www.kamtape.com/api2_rest, XML) for videos, details, comments and users, and scrapes
// the HTML results page for search. Video files: get_video?video_id=ID&webm=1 is MP4, with range support.
#include <time.h>
#include <string.h>
#include <algorithm>
#include "internal_common.hpp"
#include "parser.hpp"

static const char *KT_HOST = "https://www.kamtape.com";

// ------------------------------------------------------------------------------------------------ helpers
static std::string html_decode(std::string s) {
	static const std::pair<const char *, const char *> ent[] = {{"&amp;", "&"}, {"&lt;", "<"},  {"&gt;", ">"},
	                                                           {"&quot;", "\""}, {"&#39;", "'"}, {"&apos;", "'"},
	                                                           {"&nbsp;", " "}};
	for (auto &e : ent) {
		size_t pos = 0;
		while ((pos = s.find(e.first, pos)) != std::string::npos) {
			s.replace(pos, strlen(e.first), e.second);
			pos += strlen(e.second);
		}
	}
	// numeric entities: &#039; &#x27;
	size_t p = 0;
	while ((p = s.find("&#", p)) != std::string::npos) {
		size_t semi = s.find(';', p);
		if (semi == std::string::npos || semi - p > 8) { p += 2; continue; }
		bool hex = p + 2 < semi && (s[p + 2] == 'x' || s[p + 2] == 'X');
		long cp = strtol(s.c_str() + p + (hex ? 3 : 2), nullptr, hex ? 16 : 10);
		std::string repl;
		if (cp > 0 && cp < 0x80) repl = std::string(1, (char)cp);
		else if (cp < 0x800) { repl.push_back((char)(0xC0 | (cp >> 6))); repl.push_back((char)(0x80 | (cp & 0x3F))); }
		else if (cp < 0x10000) { repl.push_back((char)(0xE0 | (cp >> 12))); repl.push_back((char)(0x80 | ((cp >> 6) & 0x3F))); repl.push_back((char)(0x80 | (cp & 0x3F))); }
		else { repl.push_back((char)(0xF0 | (cp >> 18))); repl.push_back((char)(0x80 | ((cp >> 12) & 0x3F))); repl.push_back((char)(0x80 | ((cp >> 6) & 0x3F))); repl.push_back((char)(0x80 | (cp & 0x3F))); }
		s.replace(p, semi - p + 1, repl);
		p += repl.size();
	}
	return s;
}
static std::string strip_cdata(const std::string &s) {
	if (s.compare(0, 9, "<![CDATA[") == 0 && s.size() >= 12) return s.substr(9, s.size() - 12);
	return html_decode(s);
}
// text of the first <tag>...</tag> at or after `from` (inside [from, to)); `*end` receives the position after the closing tag
static std::string xml_text(const std::string &x, const char *tag, size_t from = 0, size_t to = std::string::npos,
                            size_t *end = nullptr) {
	std::string open = std::string("<") + tag + ">", close = std::string("</") + tag + ">";
	size_t a = x.find(open, from);
	if (a == std::string::npos || (to != std::string::npos && a >= to)) return "";
	a += open.size();
	size_t b = x.find(close, a);
	if (b == std::string::npos) return "";
	if (end) *end = b + close.size();
	return strip_cdata(x.substr(a, b - a));
}
static std::string url_encode(const std::string &s) {
	static const char *hex = "0123456789ABCDEF";
	std::string r;
	for (unsigned char c : s) {
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') r.push_back(c);
		else { r.push_back('%'); r.push_back(hex[c >> 4]); r.push_back(hex[c & 15]); }
	}
	return r;
}
static std::string time_ago(long long unix_time) {
	long long diff = (long long)time(nullptr) - unix_time;
	if (unix_time <= 0 || diff < 0) return "";
	struct { long long secs; const char *name; } units[] = {{31536000, "year"}, {2592000, "month"}, {86400, "day"},
	                                                         {3600, "hour"},    {60, "minute"}};
	for (auto &u : units) {
		if (diff >= u.secs) {
			long long n = diff / u.secs;
			return std::to_string(n) + " " + u.name + (n == 1 ? "" : "s") + " ago";
		}
	}
	return "just now";
}
static std::string duration_text(int seconds) {
	char buf[32];
	if (seconds >= 3600) snprintf(buf, sizeof(buf), "%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
	else snprintf(buf, sizeof(buf), "%d:%02d", seconds / 60, seconds % 60);
	return buf;
}
static std::string views_text(const std::string &n) { return n.empty() ? "" : n + " views"; }
static std::string api_url(const std::string &method, const std::string &params = "") {
	return std::string(KT_HOST) + "/api2_rest?method=" + method + (params.empty() ? "" : "&" + params);
}
std::string kamtape_video_url(const std::string &id) { return std::string(KT_HOST) + "/watch?v=" + id; }
std::string kamtape_thumbnail_url(const std::string &id) { return std::string(KT_HOST) + "/get_still?video_id=" + id; }
std::string kamtape_stream_url(const std::string &id) { return std::string(KT_HOST) + "/get_video?video_id=" + id + "&webm=1"; }

static YouTubeVideoSuccinct parse_api_video(const std::string &x, size_t from, size_t to) {
	YouTubeVideoSuccinct v;
	std::string id = xml_text(x, "id", from, to);
	v.url = kamtape_video_url(id);
	v.title = xml_text(x, "title", from, to);
	v.author = xml_text(x, "author", from, to);
	v.duration_text = duration_text(atoi(xml_text(x, "length_seconds", from, to).c_str()));
	v.views_str = views_text(xml_text(x, "view_count", from, to));
	v.publish_date = time_ago(atoll(xml_text(x, "upload_time", from, to).c_str()));
	v.thumbnail_url = kamtape_thumbnail_url(id);
	return v;
}
// parses <video>...</video> blocks of a *_list response
static std::vector<YouTubeVideoSuccinct> parse_api_video_list(const std::string &x) {
	std::vector<YouTubeVideoSuccinct> res;
	size_t pos = 0;
	while ((pos = x.find("<video>", pos)) != std::string::npos) {
		size_t end = x.find("</video>", pos);
		if (end == std::string::npos) break;
		res.push_back(parse_api_video(x, pos, end));
		pos = end + 8;
	}
	return res;
}
static std::string api_error(const std::string &x) {
	if (x.find("status=\"fail\"") != std::string::npos) return xml_text(x, "description");
	return "";
}

// ------------------------------------------------------------------------------------------------ home
YouTubeHomeResult youtube_load_home_page() {
	YouTubeHomeResult res;
	auto r = http_get(api_url("kamtape.videos.list_featured"));
	if (!r.first) { res.error = "Couldn't reach Kamtape: " + r.second; return res; }
	if (std::string e = api_error(r.second); e != "") { res.error = e; return res; }
	res.videos = parse_api_video_list(r.second);
	if (res.videos.empty()) res.error = "No videos returned";
	return res;
}
void YouTubeHomeResult::load_more_results() {}

// ------------------------------------------------------------------------------------------------ search
// continue_token = "<query>\n<next page number>"
static YouTubeSearchResult kamtape_search(const std::string &query, int page) {
	YouTubeSearchResult res;
	std::string url = std::string(KT_HOST) + "/results?search_type=search_videos&search_query=" + url_encode(query) +
	                  "&search_sort=relevance&search_category=0&page=" + std::to_string(page);
	auto r = http_get(url);
	if (!r.first) { res.error = "Couldn't reach Kamtape: " + r.second; return res; }
	const std::string &h = r.second;
	size_t pos = 0;
	while ((pos = h.find("class=\"vEntry\"", pos)) != std::string::npos) {
		size_t end = h.find("<!-- end vEntry -->", pos);
		if (end == std::string::npos) end = h.size();
		YouTubeVideoSuccinct v;
		size_t w = h.find("/watch?v=", pos);
		if (w == std::string::npos || w > end) break;
		std::string id = h.substr(w + 9, 11);
		v.url = kamtape_video_url(id);
		v.thumbnail_url = kamtape_thumbnail_url(id);
		size_t t = h.find("class=\"vtitle\"", pos);
		if (t != std::string::npos && t < end) {
			size_t a = h.find('>', h.find("<a ", t)) + 1, b = h.find("</a>", a);
			if (b != std::string::npos) v.title = html_decode(h.substr(a, b - a));
			size_t rt = h.find("class=\"runtime\">", t);
			if (rt != std::string::npos && rt < end) {
				rt += 16;
				v.duration_text = h.substr(rt, h.find('<', rt) - rt);
			}
		}
		size_t ad = h.find("Added:</span>", pos);
		if (ad != std::string::npos && ad < end) {
			ad += 13;
			std::string s = h.substr(ad, h.find('<', ad) - ad);
			s.erase(0, s.find_first_not_of(" \t\r\n"));
			s.erase(s.find_last_not_of(" \t\r\n") + 1);
			v.publish_date = s;
		}
		size_t vw = h.find("Views:</span>", pos);
		if (vw != std::string::npos && vw < end) {
			vw += 13;
			std::string s = h.substr(vw, h.find('<', vw) - vw);
			s.erase(0, s.find_first_not_of(" \t\r\n"));
			s.erase(s.find_last_not_of(" \t\r\n") + 1);
			v.views_str = views_text(s);
		}
		res.results.push_back(YouTubeSuccinctItem(v));
		pos = end;
	}
	if (res.results.empty() && page == 1) res.error = "No results";
	if (h.find(">Next</a>") != std::string::npos && !res.results.empty()) res.continue_token = query + "\n" + std::to_string(page + 1);
	return res;
}
YouTubeSearchResult youtube_load_search(std::string url) {
	auto p = url.find("search_query=");
	std::string q = p == std::string::npos ? "" : url_decode(url.substr(p + 13, url.find('&', p) == std::string::npos ? std::string::npos : url.find('&', p) - p - 13));
	std::replace(q.begin(), q.end(), '+', ' ');
	return kamtape_search(q, 1);
}
void YouTubeSearchResult::load_more_results() {
	auto nl = continue_token.find('\n');
	if (nl == std::string::npos) { continue_token = ""; return; }
	auto more = kamtape_search(continue_token.substr(0, nl), atoi(continue_token.substr(nl + 1).c_str()));
	results.insert(results.end(), more.results.begin(), more.results.end());
	continue_token = more.continue_token;
}

// ------------------------------------------------------------------------------------------------ video page
YouTubeVideoDetail youtube_load_video_page(std::string url) {
	YouTubeVideoDetail res;
	res.is_livestream = false;
	res.is_upcoming = false;
	res.duration_ms = 0;
	res.comment_continue_type = -1;
	res.comments_disabled = false;
	res.url = url;
	std::string id = youtube_get_video_id_by_url(url);
	if (id == "" && youtube_is_valid_video_id(url)) id = url;
	if (id == "") { res.error = "Invalid video URL"; return res; }
	res.id = id;
	auto r = http_get(api_url("kamtape.videos.get_details", "video_id=" + id));
	if (!r.first) { res.error = "Couldn't reach Kamtape: " + r.second; return res; }
	const std::string &x = r.second;
	if (std::string e = api_error(x); e != "") { res.error = e; return res; }

	res.title = xml_text(x, "title");
	res.description = xml_text(x, "description");
	res.author.name = xml_text(x, "author");
	res.author.id = res.author.name;
	res.views_str = views_text(xml_text(x, "view_count"));
	res.publish_date = time_ago(atoll(xml_text(x, "upload_time").c_str()));
	res.duration_ms = atoi(xml_text(x, "length_seconds").c_str()) * 1000;
	std::string avg = xml_text(x, "rating_avg"), cnt = xml_text(x, "rating_count");
	if (!avg.empty()) {
		char buf[64];
		snprintf(buf, sizeof(buf), "%.1f/5 (%s)", atof(avg.c_str()), cnt.c_str());
		res.like_count_str = buf;
	}
	res.succinct_thumbnail_url = kamtape_thumbnail_url(id);
	res.both_stream_url = kamtape_stream_url(id);
	res.audio_stream_url = res.both_stream_url; // audio-only mode plays the same file
	res.playability_status = "OK";

	// comments
	size_t cl = x.find("<comment_list>");
	size_t pos = cl;
	while (cl != std::string::npos && (pos = x.find("<comment>", pos)) != std::string::npos) {
		size_t end = x.find("</comment>", pos);
		if (end == std::string::npos) break;
		YouTubeVideoDetail::Comment c;
		c.author.name = xml_text(x, "author", pos, end);
		c.author.id = c.author.name;
		c.content = xml_text(x, "text", pos, end);
		c.publish_date = time_ago(atoll(xml_text(x, "time", pos, end).c_str()));
		c.reply_num = 0;
		res.comments.push_back(c);
		pos = end + 10;
	}

	// related videos: other videos with the first tag, falling back to the featured list
	std::string tags = xml_text(x, "tags");
	std::string first_tag = tags.substr(0, tags.find(' '));
	std::vector<YouTubeVideoSuccinct> related;
	if (!first_tag.empty()) {
		auto t = http_get(api_url("kamtape.videos.list_by_tag", "tag=" + url_encode(first_tag)));
		if (t.first) related = parse_api_video_list(t.second);
	}
	if (related.size() < 3) {
		auto f = http_get(api_url("kamtape.videos.list_featured"));
		if (f.first) {
			auto more = parse_api_video_list(f.second);
			related.insert(related.end(), more.begin(), more.end());
		}
	}
	for (auto &v : related) {
		if (youtube_get_video_id_by_url(v.url) == id) continue;
		res.suggestions.push_back(YouTubeSuccinctItem(v));
	}
	return res;
}
void YouTubeVideoDetail::load_more_suggestions() { suggestions_continue_token = ""; }
void YouTubeVideoDetail::load_more_comments() { comment_continue_type = -1; }
void YouTubeVideoDetail::load_caption(const std::string &, const std::string &) {}
void YouTubeVideoDetail::Comment::load_more_replies() { replies_continue_token = ""; }

// ------------------------------------------------------------------------------------------------ users ("channels")
YouTubeChannelDetail youtube_load_channel_page(std::string url_or_id) {
	YouTubeChannelDetail res;
	std::string user = url_or_id;
	auto p = user.find("user=");
	if (p != std::string::npos) user = user.substr(p + 5);
	res.id = user;
	res.url = res.url_original = std::string(KT_HOST) + "/user/" + user;
	auto prof = http_get(api_url("kamtape.users.get_profile", "user=" + url_encode(user)));
	if (!prof.first) { res.error = "Couldn't reach Kamtape: " + prof.second; return res; }
	if (std::string e = api_error(prof.second); e != "") { res.error = e; return res; }
	res.name = user;
	res.description = xml_text(prof.second, "about_me");
	res.subscriber_count_str = xml_text(prof.second, "video_upload_count") + " videos";
	auto vids = http_get(api_url("kamtape.videos.list_by_user", "user=" + url_encode(user)));
	if (vids.first) res.videos = parse_api_video_list(vids.second);
	return res;
}
YouTubeChannelDetail youtube_load_channel_streams_page(std::string url_or_id) { return youtube_load_channel_page(url_or_id); }
YouTubeChannelDetail youtube_load_channel_shorts_page(std::string url_or_id) { return youtube_load_channel_page(url_or_id); }
std::vector<YouTubeChannelDetail> youtube_load_channel_page_multi(std::vector<std::string> ids,
                                                                  std::function<void(int, int)> progress) {
	std::vector<YouTubeChannelDetail> res;
	for (size_t i = 0; i < ids.size(); i++) {
		res.push_back(youtube_load_channel_page(ids[i]));
		if (progress) progress((int)i + 1, (int)ids.size());
	}
	return res;
}
void YouTubeChannelDetail::load_more_videos() { videos_continue_token = ""; }
void YouTubeChannelDetail::load_more_streams() { streams_continue_token = ""; }
void YouTubeChannelDetail::load_more_shorts() { shorts_continue_token = ""; }
void YouTubeChannelDetail::load_playlists() {}
void YouTubeChannelDetail::load_more_community_posts() { community_continuation_token = ""; }

void youtube_change_content_language(std::string) {}
