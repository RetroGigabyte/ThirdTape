// Kamtape data layer (replaces the former YouTube parser behind parser.hpp).
// Uses Kamtape's REST API (https://www.kamtape.com/api2_rest, XML) for videos, details, comments and users, and scrapes
// the HTML results page for search. Video files: get_video?video_id=ID&webm=1 is MP4, with range support.
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
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


static std::string strip_tags(const std::string &in); // defined below (profiles section)
static std::string trim_ws(std::string s);

// ------------------------------------------------------------------------------------------------ account (login)
// Login is a plain form (POST /login: username, password). The session lives in cookies, which are kept in memory and
// saved to <data dir>/account.txt (never the password). The HTTP layer adds them to every kamtape.com request.
static std::map<std::string, std::string> cookie_jar;
static std::string logged_username;
static LightLock account_lock;
static bool account_lock_inited = false;
static void account_lock_init() {
	if (!account_lock_inited) {
		LightLock_Init(&account_lock);
		account_lock_inited = true;
	}
}
static std::string account_file() { return "sdmc:" + DEF_MAIN_DIR + "account.txt"; }

std::string kamtape_cookie_for(const std::string &url) {
	if (url.find("kamtape.com") == std::string::npos) return "";
	account_lock_init();
	LightLock_Lock(&account_lock);
	std::string res;
	for (auto &c : cookie_jar) res += (res.empty() ? "" : "; ") + c.first + "=" + c.second;
	LightLock_Unlock(&account_lock);
	return res;
}
// takes the Set-Cookie lines of a response into the jar (the caller holds no lock)
static void absorb_cookies(NetworkResult &r) {
	std::string all = r.get_header("set-cookie");
	size_t pos = 0;
	account_lock_init();
	LightLock_Lock(&account_lock);
	while (pos < all.size()) {
		size_t nl = all.find('\n', pos);
		std::string line = all.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
		pos = nl == std::string::npos ? all.size() : nl + 1;
		std::string pair = line.substr(0, line.find(';'));
		size_t eq = pair.find('=');
		if (eq == std::string::npos || eq == 0) continue;
		std::string name = pair.substr(0, eq), value = pair.substr(eq + 1);
		if (value.empty() || value == "deleted") cookie_jar.erase(name);
		else cookie_jar[name] = value;
	}
	LightLock_Unlock(&account_lock);
}
static void save_account() {
	account_lock_init();
	LightLock_Lock(&account_lock);
	std::string out = logged_username + "\n";
	for (auto &c : cookie_jar) out += c.first + "=" + c.second + "\n";
	LightLock_Unlock(&account_lock);
	mkdir(("sdmc:" + DEF_MAIN_DIR).c_str(), 0777);
	if (FILE *f = fopen(account_file().c_str(), "wb")) {
		fwrite(out.data(), 1, out.size(), f);
		fclose(f);
	}
}
void kamtape_account_init() {
	account_lock_init();
	FILE *f = fopen(account_file().c_str(), "rb");
	if (!f) return;
	std::string data;
	char buf[512];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, n);
	fclose(f);
	LightLock_Lock(&account_lock);
	size_t pos = 0;
	bool first = true;
	while (pos < data.size()) {
		size_t nl = data.find('\n', pos);
		std::string line = data.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
		pos = nl == std::string::npos ? data.size() : nl + 1;
		if (first) { logged_username = line; first = false; continue; }
		size_t eq = line.find('=');
		if (eq != std::string::npos && eq > 0) cookie_jar[line.substr(0, eq)] = line.substr(eq + 1);
	}
	if (cookie_jar.empty()) logged_username.clear();
	LightLock_Unlock(&account_lock);
}
bool kamtape_logged_in() {
	account_lock_init();
	LightLock_Lock(&account_lock);
	bool in = !logged_username.empty() && !cookie_jar.empty();
	LightLock_Unlock(&account_lock);
	return in;
}
std::string kamtape_username() {
	account_lock_init();
	LightLock_Lock(&account_lock);
	std::string u = logged_username;
	LightLock_Unlock(&account_lock);
	return u;
}
void kamtape_logout() {
	account_lock_init();
	LightLock_Lock(&account_lock);
	cookie_jar.clear();
	logged_username.clear();
	LightLock_Unlock(&account_lock);
	remove(account_file().c_str());
}
std::string kamtape_login(const std::string &user, const std::string &pass) {
	if (user.empty() || pass.empty()) return "Enter a username and a password.";
	// start from a clean session, like a browser that has just opened the login page
	account_lock_init();
	LightLock_Lock(&account_lock);
	cookie_jar.clear();
	logged_username.clear();
	LightLock_Unlock(&account_lock);

	auto page = thread_network_session_list.perform(http_get_request(std::string(KT_HOST) + "/login"));
	if (page.fail) return "Couldn't reach Kamtape.";
	absorb_cookies(page);

	std::string body = "current_form=loginForm&username=" + url_encode(user) + "&password=" + url_encode(pass) +
	                   "&remember=1&action_login=Log+In";
	std::map<std::string, std::string> headers = {{"Content-Type", "application/x-www-form-urlencoded"},
	                                              {"Referer", std::string(KT_HOST) + "/login"}};
	auto res = thread_network_session_list.perform(HttpRequest::POST(std::string(KT_HOST) + "/login", headers, body));
	if (res.fail) return "Couldn't reach Kamtape.";
	absorb_cookies(res);
	std::string html(res.data.begin(), res.data.end());

	// a failed login answers with the form again and an error message (e.g. "Please check your username.")
	size_t eb = html.find("class=\"error");
	if (eb != std::string::npos) {
		size_t gt = html.find('>', eb), end = html.find("</div>", eb);
		if (gt != std::string::npos && end != std::string::npos && end > gt) {
			std::string msg = trim_ws(html_decode(strip_tags(html.substr(gt + 1, end - gt - 1))));
			if (!msg.empty()) {
				kamtape_logout();
				return msg;
			}
		}
	}
	if (cookie_jar.empty()) return "Login failed.";

	// the cookies must now give access to the account page (it shows the login form again when they don't)
	auto check = thread_network_session_list.perform(http_get_request(std::string(KT_HOST) + "/my_account"));
	if (check.fail) return "Couldn't reach Kamtape.";
	std::string account_html(check.data.begin(), check.data.end());
	if (account_html.find("name=\"password\"") != std::string::npos) {
		kamtape_logout();
		return "Login failed. Please check your username and password.";
	}
	account_lock_init();
	LightLock_Lock(&account_lock);
	logged_username = user;
	LightLock_Unlock(&account_lock);
	save_account();
	return "";
}

// ------------------------------------------------------------------------------------------------ profiles (scraped)
// The API returns an empty profile text, so everything comes from https://www.kamtape.com/profile?user=<name>:
// the picture (first image of the profile box, a 4:3 thumbnail), the bio, the labelled fields and the counters.
static std::string strip_tags(const std::string &in) {
	std::string out;
	bool tag = false;
	for (char c : in) {
		if (c == '<') tag = true;
		else if (c == '>') tag = false;
		else if (!tag) out.push_back(c);
	}
	return out;
}
static std::string trim_ws(std::string s) {
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return "";
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}
struct KtProfile {
	std::string icon, description, subscribers, views, uploads;
};
static KtProfile kamtape_profile(const std::string &user) {
	static std::map<std::string, KtProfile> cache;
	static LightLock lock;
	static bool lock_inited = false;
	if (!lock_inited) {
		LightLock_Init(&lock);
		lock_inited = true;
	}
	LightLock_Lock(&lock);
	auto it = cache.find(user);
	if (it != cache.end()) {
		KtProfile cached = it->second;
		LightLock_Unlock(&lock);
		return cached;
	}
	LightLock_Unlock(&lock);

	KtProfile pr;
	auto r = http_get(std::string(KT_HOST) + "/profile?user=" + url_encode(user));
	if (r.first) {
		const std::string &h = r.second;
		size_t box = h.find("id=\"pBox\"");
		if (box != std::string::npos) {
			size_t box_end = h.find("end pBox", box);
			if (box_end == std::string::npos) box_end = h.size();
			// picture
			size_t img = h.find("<img src=\"", box);
			if (img != std::string::npos && img < box_end) {
				img += 10;
				size_t end = h.find('"', img);
				if (end != std::string::npos) pr.icon = h.substr(img, end - img);
				if (!pr.icon.empty() && pr.icon[0] == '/') pr.icon = std::string(KT_HOST) + pr.icon;
			}
			// counters: <span class="smallText">Subscribers:</span> <b>9</b>
			auto counter = [&](const char *label) {
				size_t p = h.find(std::string(label) + ":</span>", box);
				if (p == std::string::npos || p > box_end) return std::string();
				size_t bs = h.find("<b>", p), be = h.find("</b>", p);
				if (bs == std::string::npos || be == std::string::npos) return std::string();
				return trim_ws(html_decode(h.substr(bs + 3, be - bs - 3)));
			};
			pr.subscribers = counter("Subscribers");
			pr.views = counter("Channel Views");

			// description = bio paragraph + the labelled fields
			std::string text;
			size_t gender = h.find("Gender: ", box);
			size_t bio_from = gender != std::string::npos && gender < box_end ? gender : box;
			size_t bio = h.find("<div class=\"padT3\">", bio_from);
			if (bio != std::string::npos && bio < box_end) {
				bio += 20;
				size_t bio_end = h.find("</div>", bio);
				if (bio_end != std::string::npos) {
					std::string raw = h.substr(bio, bio_end - bio);
					if (raw.find('<') == std::string::npos) text = trim_ws(html_decode(raw));
				}
			}
			std::string fields;
			static const char *labels[] = {"Name", "Age", "Gender", "Country", "City", "Hometown", "Schools", "Occupations", "Companies",
			                              "Interests &amp; Hobbies", "Favorite Movies &amp; Shows", "Favorite Music", "Favorite Books"};
			for (const char *label : labels) {
				std::string needle = std::string("<span class=\"smallText\">") + label + ": </span>";
				size_t p = h.find(needle, box);
				if (p == std::string::npos || p > box_end) continue;
				size_t vs = h.find("<b>", p), ve = h.find("</b>", p);
				if (vs == std::string::npos || ve == std::string::npos || vs > p + needle.size() + 20) continue;
				std::string val = trim_ws(html_decode(strip_tags(h.substr(vs + 3, ve - vs - 3))));
				if (!val.empty()) fields += html_decode(label) + ": " + val + "\n";
			}
			size_t web = h.find("<span class=\"smallText\">Website: </span>", box);
			if (web != std::string::npos && web < box_end) {
				size_t hs = h.find("href=\"", web);
				if (hs != std::string::npos) {
					hs += 6;
					fields += "Website: " + html_decode(h.substr(hs, h.find('"', hs) - hs)) + "\n";
				}
			}
			size_t joined = h.find("Joined: <b>", box);
			if (joined != std::string::npos && joined < box_end) {
				joined += 11;
				fields += "Joined: " + trim_ws(h.substr(joined, h.find("</b>", joined) - joined)) + "\n";
			}
			pr.description = text;
			if (!fields.empty()) pr.description += (text.empty() ? "" : "\n\n") + trim_ws(fields);
		}
	}
	LightLock_Lock(&lock);
	cache[user] = pr;
	LightLock_Unlock(&lock);
	return pr;
}
static std::string kamtape_profile_picture(const std::string &user) { return kamtape_profile(user).icon; }


// ------------------------------------------------------------------------------------------------ comments (scraped)
// Comments with scores and replies come from the HTML: the watch page has the first ones, and
// /comment_servlet?all_comments&v=<id> has all of them. Replies (class commentEntryReply) follow their comment.
static std::string comment_text(std::string raw) {
	for (const char *br : {"<br />", "<br/>", "<br>"}) {
		size_t p = 0;
		while ((p = raw.find(br, p)) != std::string::npos) {
			raw.replace(p, strlen(br), "\n");
			p += 1;
		}
	}
	return trim_ws(html_decode(strip_tags(raw)));
}
static std::vector<YouTubeVideoDetail::Comment> parse_comments_html(const std::string &h) {
	std::vector<YouTubeVideoDetail::Comment> out;
	std::vector<std::pair<size_t, bool>> marks; // position, is_reply
	size_t pos = 0;
	while ((pos = h.find("class=\"commentEntry", pos)) != std::string::npos) {
		char c = h[pos + 19];
		if (c == '"') marks.push_back({pos, false});
		else if (h.compare(pos + 19, 6, "Reply\"") == 0) marks.push_back({pos, true});
		pos += 19;
	}
	for (size_t i = 0; i < marks.size(); i++) {
		size_t end = i + 1 < marks.size() ? marks[i + 1].first : h.size();
		std::string b = h.substr(marks[i].first, end - marks[i].first);
		YouTubeVideoDetail::Comment c;
		c.reply_num = 0;
		size_t idp = b.find("id=\"comment_");
		if (idp != std::string::npos) {
			idp += 12;
			c.id = b.substr(idp, b.find('"', idp) - idp);
		}
		size_t ap = b.find("<b><a href=\"/user/");
		if (ap != std::string::npos) {
			ap += 18;
			c.author.name = b.substr(ap, b.find('"', ap) - ap);
			c.author.id = c.author.name;
		}
		size_t tp = b.find("<span class=\"smallText\"> (");
		if (tp != std::string::npos) {
			tp += 26;
			c.publish_date = trim_ws(b.substr(tp, b.find(")", tp) - tp));
		}
		if (!c.id.empty()) {
			size_t sp = b.find("id=\"comment_score_" + c.id + "\"");
			if (sp != std::string::npos) {
				sp = b.find('>', sp);
				if (sp != std::string::npos) c.upvotes_str = trim_ws(html_decode(b.substr(sp + 1, b.find('<', sp) - sp - 1)));
			}
		}
		size_t bp = b.find("class=\"commentBody marL8 normalText\"");
		if (bp != std::string::npos) {
			bp = b.find('>', bp);
			size_t be = b.find("</div>", bp);
			if (bp != std::string::npos && be != std::string::npos) c.content = comment_text(b.substr(bp + 1, be - bp - 1));
		}
		if (c.content.empty() && c.author.name.empty()) continue;
		if (marks[i].second) {
			if (!out.empty()) {
				out.back().replies.push_back(c);
				out.back().reply_num++;
			}
		} else {
			out.push_back(c);
		}
	}
	return out;
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
	res.author.icon_url = kamtape_profile_picture(res.author.name);
	res.views_str = views_text(xml_text(x, "view_count"));
	res.publish_date = time_ago(atoll(xml_text(x, "upload_time").c_str()));
	res.duration_ms = atoi(xml_text(x, "length_seconds").c_str()) * 1000;
	res.rating_avg = (float)atof(xml_text(x, "rating_avg").c_str());
	res.rating_count = atoi(xml_text(x, "rating_count").c_str());
	res.succinct_thumbnail_url = kamtape_thumbnail_url(id);
	res.both_stream_url = kamtape_stream_url(id);
	res.audio_stream_url = res.both_stream_url; // audio-only mode plays the same file
	res.playability_status = "OK";

	// comments: first ones with scores from the watch page (HTML); the API's comments (no scores) are the fallback
	{
		auto wp = http_get(kamtape_video_url(id));
		if (wp.first) {
			res.comments = parse_comments_html(wp.second);
			if (wp.second.find("comment_servlet?all_comments") != std::string::npos) res.comment_continue_type = 0;
		}
	}
	if (res.comments.empty()) {
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
	}

	// playlist context (the URL carries &list=<playlist id>)
	std::string list_id = youtube_get_playlist_id_by_url(url);
	if (!list_id.empty()) {
		auto pl = http_get(std::string(KT_HOST) + "/view_play_list?p=" + url_encode(list_id));
		if (pl.first) {
			const std::string &h = pl.second;
			res.playlist.id = list_id;
			res.playlist.selected_index = 0;
			size_t tp = h.find("<span>Playlist: ");
			if (tp != std::string::npos) {
				tp += 16;
				res.playlist.title = html_decode(h.substr(tp, h.find("</span>", tp) - tp));
			}
			size_t pos = 0;
			while ((pos = h.find("class=\"vDetailEntry\"", pos)) != std::string::npos) {
				size_t end = h.find("class=\"vDetailEntry\"", pos + 10);
				if (end == std::string::npos) end = h.size();
				std::string e = h.substr(pos, end - pos);
				pos = end;
				YouTubeVideoSuccinct v;
				size_t w = e.find("/watch?v=");
				if (w == std::string::npos) continue;
				std::string vid = e.substr(w + 9, 11);
				v.url = std::string(KT_HOST) + "/watch?v=" + vid + "&list=" + list_id;
				v.thumbnail_url = kamtape_thumbnail_url(vid);
				size_t t = e.find("class=\"title\"");
				if (t != std::string::npos) {
					size_t a = e.find('>', e.find("<a ", t)) + 1, b = e.find("</a>", a);
					if (b != std::string::npos) v.title = html_decode(e.substr(a, b - a));
					size_t rt = e.find("class=\"runtime\">", t);
					if (rt != std::string::npos) {
						rt += 16;
						v.duration_text = e.substr(rt, e.find('<', rt) - rt);
					}
				}
				size_t fr = e.find("From:</span> <a href=\"/user/");
				if (fr != std::string::npos) {
					fr += 28;
					v.author = e.substr(fr, e.find('"', fr) - fr);
				}
				size_t ad = e.find("Added:</span>");
				if (ad != std::string::npos) {
					ad += 13;
					v.publish_date = trim_ws(e.substr(ad, e.find('<', ad) - ad));
				}
				size_t vw = e.find("Views:</span>");
				if (vw != std::string::npos) {
					vw += 13;
					v.views_str = views_text(trim_ws(e.substr(vw, e.find('<', vw) - vw)));
				}
				res.playlist.videos.push_back(v);
				if (vid == id) res.playlist.selected_index = (int)res.playlist.videos.size() - 1;
			}
			res.playlist.total_videos = (int)res.playlist.videos.size();
			if (!res.playlist.videos.empty()) res.playlist.author_name = res.playlist.videos[0].author;
			if (res.playlist.videos.empty()) res.playlist = YouTubeVideoDetail::Playlist();
		}
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
// Loads the complete comment list and appends the comments that are not shown yet
void YouTubeVideoDetail::load_more_comments() {
	comment_continue_type = -1;
	auto r = http_get(std::string(KT_HOST) + "/comment_servlet?all_comments&v=" + id + "&fromurl=/watch?v=" + id);
	if (!r.first) return;
	auto all = parse_comments_html(r.second);
	std::map<std::string, bool> have;
	for (auto &c : comments) have[c.id] = true;
	for (auto &c : all) {
		if (!c.id.empty() && !have.count(c.id)) comments.push_back(c);
	}
}
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
	KtProfile pr = kamtape_profile(user);
	res.icon_url = pr.icon;
	res.description = pr.description;
	res.subscriber_count_str = xml_text(prof.second, "video_upload_count") + " videos";
	if (!pr.subscribers.empty()) res.subscriber_count_str += " \xE2\x80\xA2 " + pr.subscribers + " subscribers";
	// tells the UI that the Playlists tab can be loaded (see load_playlists())
	res.playlist_tab_browse_id = user;
	res.playlist_tab_params = "kamtape";
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
// Playlists of a user: https://www.kamtape.com/profile_play_list?user=<name>
void YouTubeChannelDetail::load_playlists() {
	std::string user = playlist_tab_browse_id;
	playlist_tab_browse_id = "";
	playlist_tab_params = "";
	playlists.clear();
	if (user.empty()) return;
	auto r = http_get(std::string(KT_HOST) + "/profile_play_list?user=" + url_encode(user));
	if (!r.first) return;
	const std::string &h = r.second;
	std::vector<YouTubePlaylistSuccinct> items;
	size_t pos = 0;
	while ((pos = h.find("<table class=\"playlist\"", pos)) != std::string::npos) {
		// an entry runs until the next one (it contains nested tables for the thumbnail stack)
		size_t next = h.find("<table class=\"playlist\"", pos + 10);
		size_t end = next == std::string::npos ? h.size() : next;
		std::string e = h.substr(pos, end - pos);
		pos = end;
		YouTubePlaylistSuccinct pl;
		size_t pid = e.find("/view_play_list?p=");
		if (pid == std::string::npos) continue;
		pid += 18;
		std::string id = e.substr(pid, e.find('"', pid) - pid);
		size_t first = e.find("/watch?v=");
		std::string first_video = first == std::string::npos ? "" : e.substr(first + 9, 11);
		size_t img = e.find("<img src=\"");
		if (img != std::string::npos) {
			img += 10;
			pl.thumbnail_url = e.substr(img, e.find('"', img) - img);
		}
		size_t t = e.find("class=\"title\"");
		if (t != std::string::npos) {
			size_t a = e.find('>', e.find("<a ", t)) + 1, b = e.find("</a>", a);
			if (b != std::string::npos) pl.title = html_decode(e.substr(a, b - a));
			size_t f = e.find("class=\"facets\">", t);
			if (f != std::string::npos) {
				f += 15;
				pl.video_count_str = trim_ws(e.substr(f, e.find('<', f) - f));
			}
		}
		if (first_video.empty()) continue;
		pl.url = std::string(KT_HOST) + "/watch?v=" + first_video + "&list=" + id;
		items.push_back(pl);
	}
	if (!items.empty()) playlists.push_back({"Playlists", items});
}
void YouTubeChannelDetail::load_more_community_posts() { community_continuation_token = ""; }

void youtube_change_content_language(std::string) {}
