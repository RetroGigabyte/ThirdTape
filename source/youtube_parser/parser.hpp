#pragma once
#include <vector>
#include <string>
#include <map>
#include <functional>

struct YouTubeChannelSuccinct {
	std::string name;
	std::string id;
	std::string icon_url;
	std::string subscribers;
	std::string video_num;
	bool is_collaboration = false;
};
struct YouTubeVideoSuccinct {
	std::string url;
	std::string title;
	std::string duration_text;
	std::string publish_date;
	std::string views_str;
	std::string author;
	std::string thumbnail_url;
};
struct YouTubePlaylistSuccinct {
	std::string url;
	std::string title;
	std::string video_count_str;
	std::string thumbnail_url;
};

struct YouTubeSuccinctItem {
	// TODO : use union or std::variant
	enum { VIDEO, CHANNEL, PLAYLIST } type;
	YouTubeVideoSuccinct video;
	YouTubeChannelSuccinct channel;
	YouTubePlaylistSuccinct playlist;

	YouTubeSuccinctItem() = default;
	YouTubeSuccinctItem(YouTubeVideoSuccinct video) : type(VIDEO), video(video) {}
	YouTubeSuccinctItem(YouTubeChannelSuccinct channel) : type(CHANNEL), channel(channel) {}
	YouTubeSuccinctItem(YouTubePlaylistSuccinct playlist) : type(PLAYLIST), playlist(playlist) {}

	// returns channel id in case of type == CHANNEL
	std::string get_url() const { return type == VIDEO ? video.url : type == CHANNEL ? channel.id : playlist.url; }
	std::string get_thumbnail_url() const {
		return type == VIDEO ? video.thumbnail_url : type == CHANNEL ? channel.icon_url : playlist.thumbnail_url;
	}
	std::string get_name() const {
		return type == VIDEO ? video.title : type == CHANNEL ? channel.name : playlist.title;
	}
};

struct YouTubeSearchResult {
	std::string error;
	std::string estimated_result_num;
	std::vector<YouTubeSuccinctItem> results;

	std::string continue_token;

	bool has_more_results() const { return continue_token != ""; }
	void load_more_results();
};
YouTubeSearchResult youtube_load_search(std::string url);

struct YouTubeVideoDetail {
	std::string error;
	std::string url;
	std::string title;
	std::string description;
	YouTubeChannelSuccinct author;
	bool metadata_from_android_vr = false;
	std::string id;
	std::string succinct_thumbnail_url;
	std::string audio_stream_url;
	std::map<int, std::string> video_stream_urls; // first : video size (144p, 240p, 360p ...)
	std::string both_stream_url;
	int duration_ms;
	bool is_livestream;
	enum class LivestreamType {
		PREMIERE,
		LIVESTREAM,
	};
	LivestreamType livestream_type;
	bool is_upcoming;
	std::string playability_status;
	std::string playability_reason;
	int stream_fragment_len; // used only for livestreams
	std::string like_count_str;
	float rating_avg = 0;  // Kamtape: 5-star rating average (0 if unrated)
	int rating_count = 0;  // number of ratings
	std::string kt_user_id, kt_session_token; // Kamtape: fields of the watch page's rating form (needed to rate)
	int kt_rating_form_count = 0;
	std::string dislike_count_str;
	std::string publish_date;
	std::string views_str;

	// caption-related data
	struct CaptionBaseLanguage {
		std::string name;     // e.g. "English", "Japanese"
		std::string id;       // e.g. "en", "ja"
		std::string base_url; // empty string for instances in caption_translated_languages
		bool is_translatable;
	};
	struct CaptionTranslationLanguage {
		std::string name;
		std::string id;
	};
	std::vector<CaptionBaseLanguage> caption_base_languages;
	std::vector<CaptionTranslationLanguage> caption_translation_languages;
	struct CaptionPiece {
		float start_time;
		float end_time;
		std::string content;
	};
	using Caption = std::vector<CaptionPiece>;
	std::map<std::pair<std::string, std::string>, Caption> caption_data;

	std::vector<YouTubeSuccinctItem> suggestions;
	struct Playlist {
		std::string id;
		std::string title;
		std::string author_name;
		int total_videos;
		std::vector<YouTubeVideoSuccinct> videos;
		int selected_index;
	};
	Playlist playlist;

	struct Comment {
		std::string error;
		YouTubeChannelSuccinct author;
		std::string content;
		std::string id;
		std::string publish_date;
		std::string upvotes_str;

		int reply_num;
		std::vector<Comment> replies;
		std::string replies_continue_token;
		bool has_more_replies() const { return replies_continue_token != ""; }
		void load_more_replies();
	};
	std::vector<Comment> comments;

	std::string suggestions_continue_token;
	std::string comment_continue_token;
	int comment_continue_type; // -1 : unavailable, 0 : using watch_comments, 1 : using innertube
	bool comments_disabled;

	bool has_next_video() const {
		if (playlist.videos.size() && playlist.selected_index != (int)playlist.videos.size() - 1) {
			return true;
		}
		for (auto suggestion : suggestions) {
			if (suggestion.type == YouTubeSuccinctItem::VIDEO) {
				return true;
			}
		}
		return false;
	}
	bool has_next_video_in_playlist() const {
		return playlist.videos.size() && playlist.selected_index != (int)playlist.videos.size() - 1;
	}
	YouTubeVideoSuccinct get_next_video() const {
		if (playlist.videos.size() && playlist.selected_index != (int)playlist.videos.size() - 1) {
			return playlist.videos[std::max(0, playlist.selected_index + 1)];
		}
		for (auto suggestion : suggestions) {
			if (suggestion.type == YouTubeSuccinctItem::VIDEO) {
				return suggestion.video;
			}
		}
		return YouTubeVideoSuccinct();
	}
	bool has_more_suggestions() const { return suggestions_continue_token != ""; }
	bool has_more_comments() const { return comment_continue_type != -1; }
	bool needs_timestamp_adjusting() const { return is_livestream && livestream_type == LivestreamType::PREMIERE; }
	bool is_playable() const {
		return playability_status == "OK" &&
		       (both_stream_url != "" || (audio_stream_url != "" && video_stream_urls.size()));
	}

	void load_more_suggestions();
	void load_more_comments();
	void load_caption(const std::string &base_lang_id, const std::string &translation_lang_id);
};
// this function does not load comments; call youtube_video_page_load_more_comments() if necessary
YouTubeVideoDetail youtube_load_video_page(std::string url);

struct YouTubeChannelDetail {
	std::string id;
	std::string error;
	std::string name;
	std::string handle;
	std::string url;
	std::string url_original;
	std::string icon_url;
	std::string banner_url;
	std::string description;
	std::string subscriber_count_str;
	std::vector<YouTubeVideoSuccinct> videos;
	std::vector<YouTubeVideoSuccinct> streams;
	std::vector<YouTubeVideoSuccinct> shorts;

	std::string continue_token;
	std::string videos_continue_token;
	std::string streams_continue_token;
	std::string shorts_continue_token;
	std::string video_sort_token_newest;
	std::string video_sort_token_popular;
	std::string video_sort_token_oldest;
	int current_video_sort_type = 0; // 0: newest, 1: popular, 2: oldest
	std::string streams_sort_token_newest;
	std::string streams_sort_token_popular;
	std::string streams_sort_token_oldest;
	int current_streams_sort_type = 0; // 0: newest, 1: popular, 2: oldest
	std::string shorts_sort_token_newest;
	std::string shorts_sort_token_popular;
	std::string shorts_sort_token_oldest;
	int current_shorts_sort_type = 0; // 0: newest, 1: popular, 2: oldest
	std::string playlist_tab_browse_id;
	std::string playlist_tab_params;

	std::vector<std::pair<std::string, std::vector<YouTubePlaylistSuccinct>>>
	    playlists; // {category title, list of playlists}

	struct CommunityPost {
		std::string author_name;
		std::string author_icon_url;
		std::string time;
		std::string upvotes_str;
		std::string message;
		std::string image_url; // "" if no image
		std::string poll_total_votes;
		std::vector<std::string> poll_choices;
		YouTubeVideoSuccinct video;
	};
	std::vector<CommunityPost> community_posts;
	bool community_loaded = false;
	std::string community_continuation_token;

	bool streams_loaded = false;
	bool shorts_loaded = false;

	bool has_more_videos() const { return videos_continue_token != ""; }
	bool has_more_streams() const { return streams_continue_token != ""; }
	bool has_more_shorts() const { return shorts_continue_token != ""; }
	bool has_playlists_to_load() const { return playlist_tab_browse_id != "" && playlist_tab_params != ""; }
	bool has_community_posts_to_load() const { return !community_loaded || community_continuation_token != ""; }

	void load_more_videos();
	void load_more_streams();
	void load_more_shorts();
	void load_playlists();
	void load_more_community_posts();
};
YouTubeChannelDetail youtube_load_channel_page(std::string url_or_id);
YouTubeChannelDetail youtube_load_channel_streams_page(std::string url_or_id);
YouTubeChannelDetail youtube_load_channel_shorts_page(std::string url_or_id);
std::vector<YouTubeChannelDetail> youtube_load_channel_page_multi(std::vector<std::string> ids,
                                                                  std::function<void(int, int)> progress);

struct YouTubeHomeResult {
	std::string error;
	std::vector<YouTubeVideoSuccinct> videos;
	std::string continue_token;
	std::string visitor_data;
	bool has_more_results() const { return continue_token != "" && visitor_data != ""; }
	void load_more_results();
};
YouTubeHomeResult youtube_load_home_page();

void youtube_change_content_language(std::string language_code);

/* ---------------------------- Kamtape account (kamtape.cpp) ---------------------------- */
void kamtape_account_init();                     // loads the saved session (call once at start)
bool kamtape_logged_in();
std::string kamtape_username();
// logs in with username + password; returns "" on success, otherwise a message to show the user
std::string kamtape_login(const std::string &user, const std::string &pass);
void kamtape_logout();
// posts a comment (parent_id empty) or a reply to the comment with that id; returns "" on success, otherwise a message
std::string kamtape_post_comment(const std::string &video_id, const std::string &parent_id, const std::string &text);
// subscriptions of the logged-in account (POST /subscription_center add_user / remove_user); "" on success, else a message
std::string kamtape_subscribe(const std::string &user);
std::string kamtape_unsubscribe(const std::string &user);
// copies the account's subscriptions into the local list (and removes local ones that are not on the account);
// returns "" on success. Runs several requests: call it from a worker thread.
std::string kamtape_sync_subscriptions();
bool kamtape_is_me(const std::string &user); // same name as the logged-in user (case-insensitive)
// rates the video 1-5 stars (POST /rating); "" on success, else a message. new_avg / new_count are filled (else -1)
// when the answer could be read.
std::string kamtape_rate_video(const YouTubeVideoDetail &video, int stars, float &new_avg, int &new_count);
// votes a comment up or down (GET /comment_vote); "" on success, else a message. new_score gets the score text if the
// answer contained it.
std::string kamtape_vote_comment(const std::string &video_id, const std::string &comment_id, bool up, std::string &new_score);
// channel comments: a captcha image URL to show, and the post call (comment max 255 characters)
std::string kamtape_new_captcha_url();
std::string kamtape_post_channel_comment(const std::string &user, const std::string &text, const std::string &captcha);

/* -------------------------------- utils.cpp -------------------------------- */
std::string youtube_get_video_id_by_url(const std::string &url);
std::string youtube_get_playlist_id_by_url(const std::string &url);
std::string youtube_get_video_thumbnail_url_by_id(const std::string &id);
std::string youtube_get_video_url_by_id(const std::string &id);
std::string get_video_id_from_thumbnail_url(const std::string &url);
std::string convert_webp_thumbnail_to_jpg(const std::string &url);
bool youtube_is_valid_video_id(const std::string &id);
bool is_youtube_url(const std::string &url);
bool is_youtube_thumbnail_url(const std::string &url);
int64_t extract_stream_length(const std::string &url);

enum class YouTubePageType { VIDEO, CHANNEL, SEARCH, INVALID };
YouTubePageType youtube_get_page_type(std::string url);

struct YouTubeDislikeData {
	std::string id;
	int64_t likes;
	int64_t dislikes;
	double rating;
	int64_t view_count;
};
