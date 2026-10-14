# OpenSubtitles native integration contract

Verified 13 October 2026 from the provider's live Stoplight documentation and its linked OpenAPI 3.0.3 document, version 1.0.1. The Stoplight root works through the collaborative browser even when a text fetch returns 404 or JavaScript-only content. No API credentials were created, copied from another application, or used to make authenticated requests.

Primary sources:

- [Getting started and authentication policy](https://opensubtitles.stoplight.io/docs/opensubtitles-api/e3750fd63a100-getting-started)
- [Official OpenAPI JSON](https://stoplight.io/api/v1/projects/opensubtitles/opensubtitles-api/nodes/open_api.json)
- [Login documentation](https://opensubtitles.stoplight.io/docs/opensubtitles-api/branches/master/73acf79accc0a-login)
- [Rate limits and best practices](https://opensubtitles.stoplight.io/docs/opensubtitles-api/6ef2e232095c7-best-practices)
- [Error codes](https://opensubtitles.stoplight.io/docs/opensubtitles-api/12f131ce12132-error-codes)
- [Developer consumer registration](https://www.opensubtitles.com/consumers), also available as [English consumer registration](https://www.opensubtitles.com/en/consumers)
- [Account registration](https://www.opensubtitles.com/en/users/sign_up)

## Application key and user authentication

The provider requires one application-level consumer key. Its documentation expressly prohibits making ordinary app users create their own API keys. Reframe's maintainer must register the application and supply that key through the build/developer setup. An end user's username/password is separate and must never be replaced by the developer's hardcoded account. Every API call carries `Api-Key` and a valid application/version `User-Agent`, for example `Reframe v0.4.0`. `X-User-Agent` is an alternative where setting User-Agent is impossible.

The docs permit limited anonymous downloads with the application's key. Authentication raises the quota to the user's account allowance. Do not hardcode a daily allowance in the UI; display returned quota fields. The getting-started page and download-operation security notation are inconsistent about anonymous access: the former allows anonymous quota, while the download operation warns to send both headers. For authenticated downloads, always send both `Api-Key` and `Authorization: Bearer <token>`.

Use `Accept: */*`; JSON POSTs also use `Content-Type: application/json`. Normal API base is `https://api.opensubtitles.com/api/v1`. After login, use its returned `base_url` host with HTTPS and `/api/v1`; the allowed hosts in the schema are `api.opensubtitles.com` and `vip-api.opensubtitles.com`. Always include the token on the VIP host. Token expiry should follow the JWT `exp` claim rather than a hardcoded duration.

## Login

`POST /login`, application key required.

JSON request properties:

| Field | Type | Meaning |
| --- | --- | --- |
| `username` | string | Required user's OpenSubtitles username |
| `password` | string | Required user's password |

200 response properties are required `token` string, `base_url` hostname, `status` number, and `user` object. User has `allowed_downloads`, `allowed_translations`, `user_id` numbers; `level` string; `ext_installed`, `vip` booleans. Login body is semantically required even though the OpenAPI requestBody wrapper is marked optional; its schema requires both credential properties.

## Subtitle search

`GET /subtitles`, application key required; bearer optional before login.

Core query fields:

- `query`, `languages`, `type`, `moviehash`: strings. Languages are comma-separated API codes. Hash is exactly 16 lowercase hexadecimal characters. Type is movie/episode/all.
- `id`, `imdb_id`, `tmdb_id`, `uploader_id`, `parent_feature_id`, `parent_imdb_id`, `parent_tmdb_id`, `season_number`, `episode_number`, `year`, `page`: integers.
- `hearing_impaired`, `foreign_parts_only`, `trusted_sources`, `machine_translated`, `ai_translated`, `moviehash_match`, `order_by`, `order_direction`: string filters. Check the live spec's allowed filter values.

Use a precise movie/episode ID when available. For a show, send the parent ID plus season/episode; a unique episode ID should stand alone. Hash plus a meaningful filename query is supported. Sort query parameters alphabetically, lowercase query values, remove `tt` and leading zeroes from IMDB IDs, and validate destinations. Reframe rejects redirects on credentialed API requests so credentials cannot be forwarded to another origin. Obtain valid language codes from `GET /infos/languages`, whose response is `data: [{language_code,language_name}]`.

Search response contains numeric/integer `total_pages`, `total_count`, `per_page`, `page`, and `data` array. Each row has string `id`, `type`, and `attributes`. Useful attributes:

- `language`, `release`, `upload_date`, `url`, `comments`, `subtitle_id`: strings.
- `fps`: number; `download_count`, `new_download_count`, `votes`, `ratings`, `nb_cd`: integers.
- `hearing_impaired`, `hd`, `from_trusted`, `foreign_parts_only`, `ai_translated`, `machine_translated`: booleans.
- `files`: array of `{file_id: integer, cd_number: integer, file_name: string}`.
- `feature_details`: title/parent_title/movie_name/feature_type strings and numeric feature/movie/parent IDs, year, season and episode.
- `uploader`: uploader_id integer, name/rank strings. Examples contain null values even where the schema declares a string or integer, so accept missing/null optional metadata.

A hash search can add `moviehash_match` to the result. The download identifier is `attributes.files[].file_id`, not the top-level subtitle ID. One search result may contain several CD/files; keep those choices distinct.

## Request and fetch a subtitle

`POST /download` JSON schema:

| Field | Type | Requirement |
| --- | --- | --- |
| `file_id` | int32 | Required, from the selected search file |
| `sub_format` | string | Optional format from `/infos/formats`; default SRT |
| `file_name` | string | Optional preferred output name |
| `in_fps`, `out_fps` | number | Supply both when requesting FPS conversion |
| `timeshift` | number | Optional positive/negative seconds |
| `force_download` | boolean | Optional file-response disposition mode |

200 response has required `link`, `file_name`, `message`, `reset_time`, `reset_time_utc` strings and `requests`, `remaining` numbers. Requesting this link consumes the download quota. Fetch the returned URL with GET to obtain subtitle bytes, normally UTF-8 SRT. The link lasts at most three hours; do not persist/cache the link. Retrying the already-issued file GET avoids another quota-counting POST. The signed download URL does not require copying API credentials to its origin.

For Reframe, save the received file to an appropriate subtitle cache/user destination, then store that local URI in the selected media's existing bindings and attach it through the VLC slave-track path. Treat the returned filename as a basename, not permission to write an arbitrary path. Do not overwrite an existing user subtitle silently.

## Rate limits and error handling

Default API rate is five requests/second/IP, with `/features` limited to 40 requests/10 seconds. Higher consumer plans can have different rates. Observe `ratelimit-reset`, `ratelimit-remaining`, `ratelimit-limit`, `x-ratelimit-remaining-second`, and `x-ratelimit-limit-second` when present. Login is limited to one/second, ten/minute, thirty/hour.

| Status | Provider meaning | Client behavior |
| --- | --- | --- |
| 401 login | Invalid username/password | Stop retrying those same credentials |
| 403 | Missing/invalid application key or invalid User-Agent | Fix configuration/header; do not retry blindly |
| 406 | Invalid file_id, exhausted quota, or expired/invalid token | Inspect JSON; show quota/reset or request fresh login |
| 406 empty body | Accept header can be missing/wrong | Send `Accept: */*` |
| 410 file GET | Issued link is expired/invalid | Obtain a fresh link; this may consume quota |
| 429 | Rate window exceeded | Cool down according to response limit headers |
| 5xx | Server error | Provider suggests retry after one second; use bounded retries |

An exhausted-quota response includes `requests`, `remaining`, `message`, `reset_time`, `reset_time_utc`. Error JSON commonly has `message` and sometimes numeric `status`; do not assume a success-shaped object. The getting-started text also mentions an invalid-token message accompanying 500, so inspect that message before generic server-error retries. Daily reset timing should use the server's returned fields.

The OpenAPI `security` arrays list bearer and API-key alternatives, but prose explicitly says to provide both for authenticated download/user-info calls. Implement the prose requirement rather than generating a client that omits the app key after login.

## Verification limit

The official API and its policy are verified. The native integration cannot be tested against real search/download responses until the fork has its own consumer key. That does not justify borrowing another application's published key or developer account. Optional account login can be implemented without embedding user credentials; runtime tokens and passwords should not be emitted in logs.

## Build configuration

Supply the registered application key through the `OPENSUBTITLES_APP_KEY` packaging environment variable or GitHub Actions secret. The package writes `opensubtitles-app-key.txt` beside the player. Local developers may set `OpenSubtitles/applicationKey` in their own Reframe settings. There is no publisher setup panel in the user interface. Users sign in with their own account; that account does not replace the application key.
