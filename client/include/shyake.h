#ifndef SHYAKE_H
#define SHYAKE_H

#include <stddef.h>
#include <stdint.h>

/* Opaque pointer for the library context */
typedef struct shyake_ctx shyake_ctx;

/* Configuration options provided by the CLI caller */
typedef struct {
    const char *instance_url;
    const char *config_dir;
    const char *username;
    int plain;
    int debug;
    int no_color;
} shyake_config;

/* Initialize and free the context */
shyake_ctx* shyake_init_ctx(const shyake_config *config);
void shyake_free_ctx(shyake_ctx *ctx);

/* Set the passphrase used to decrypt secret keys (NUL or empty = none) */
void shyake_set_passphrase(shyake_ctx *ctx, const char *passphrase);

/* Set the passphrase used when saving new secret keys during rotate */
void shyake_set_new_passphrase(shyake_ctx *ctx, const char *passphrase);

/*
 * Semantic error codes returned by lib functions.
 * SHYAKE_OK = 0 so existing `ret == 0` checks still work.
 */
typedef enum {
    SHYAKE_OK              =  0,
    SHYAKE_ERR             = -1, /* generic / internal */
    SHYAKE_ERR_NETWORK     = -2, /* libcurl transport failure */
    SHYAKE_ERR_HTTP        = -3, /* unexpected HTTP status */
    SHYAKE_ERR_KEY_MISMATCH= -4, /* HTTP 409: recipient's key rotated */
    SHYAKE_ERR_GONE        = -5, /* HTTP 410: recipient destroyed */
    SHYAKE_ERR_NOT_FOUND   = -6, /* HTTP 404 */
    SHYAKE_ERR_FORBIDDEN   = -7, /* HTTP 403 */
    SHYAKE_ERR_CRYPTO      = -8, /* crypto operation failed */
    SHYAKE_ERR_NO_INSTANCE = -9, /* instance URL not configured */
} shyake_err;

/* Generate local ML-KEM and ML-DSA keypairs */
int shyake_generate_keys(shyake_ctx *ctx);

/* PoW utility */
char* shyake_mint_pow(const char *resource, int bits);

/* Registration */
shyake_err shyake_register(shyake_ctx *ctx, const char *username);

/*
 * Send a message.
 * Returns SHYAKE_OK on success.
 * Returns SHYAKE_ERR_KEY_MISMATCH if recipient's key changed.
 * Returns SHYAKE_ERR_GONE if recipient no longer exists.
 */
shyake_err shyake_send(shyake_ctx *ctx, const char *recipient,
                       const char *subject, const uint8_t *body,
                       size_t body_len);

/* --- Mail list --- */

typedef struct {
    char *mail_id;
    char *party;       /* inbox: sender; sent: recipient (raw string) */
    char *subject;     /* decrypted, NULL if failed */
    int size;          /* plaintext byte count */
    int64_t timestamp; /* UNIX timestamp seconds */
    int is_sent;       /* 1 if from sent box */
} shyake_mail_entry;

typedef struct {
    shyake_mail_entry *entries;
    int count;
} shyake_mail_list;

void shyake_free_mail_list(shyake_mail_list *list);

/*
 * Fetch inbox or sent mail list.
 * type: "inbox" or "sent".
 * Returns allocated shyake_mail_list* on success, NULL on failure.
 * Caller must free with shyake_free_mail_list().
 */
shyake_mail_list* shyake_check(shyake_ctx *ctx, const char *type);

/* --- Mail detail --- */

typedef struct {
    char *mail_id;
    char *sender;
    char *recipient;
    char *subject;     /* decrypted, NULL if failed */
    char *body;        /* decrypted, NULL if failed */
    int64_t timestamp; /* UNIX timestamp seconds */
    int size;          /* plaintext byte count */
} shyake_mail_detail;

void shyake_free_mail_detail(shyake_mail_detail *d);

/*
 * Fetch full content of a single mail (decrypt body).
 * Returns allocated shyake_mail_detail* on success, NULL on failure.
 */
shyake_mail_detail* shyake_fetch(shyake_ctx *ctx, const char *mail_id);

/*
 * Fetch metadata of a single mail (no body decryption).
 * Returns allocated shyake_mail_detail* on success, NULL on failure.
 */
shyake_mail_detail* shyake_check_one(shyake_ctx *ctx, const char *mail_id);

/*
 * Delete a mail by ID.
 * Returns 0 on success, non-zero on failure.
 */
shyake_err shyake_burn(shyake_ctx *ctx, const char *mail_id);

/*
 * Block or unblock a target.
 * unblock=0 to block, unblock=1 to unblock.
 */
shyake_err shyake_block(shyake_ctx *ctx, const char *target, int unblock);

/* Rotate keypairs and upload to server */
shyake_err shyake_rotate(shyake_ctx *ctx);

/* Destroy account and all associated data */
shyake_err shyake_destroy(shyake_ctx *ctx);

/* --- Fingerprint --- */

typedef struct {
    unsigned char local_fp[32];  /* SHA-256 of known_hosts key */
    unsigned char remote_fp[32]; /* SHA-256 of server key */
    int has_local;  /* 1 if a known_hosts entry exists */
    int match;      /* 1=MATCH, 0=MISMATCH, relevant only if has_local */
} shyake_fp_result;

void shyake_free_fp_result(shyake_fp_result *r);

/*
 * Compute fingerprint of self or a remote user.
 * target_user: NULL for self, username for remote.
 * do_update: 1 to rewrite known_hosts with fetched key.
 * Returns allocated shyake_fp_result* on success, NULL on failure.
 * Self-fingerprint: only remote_fp is populated (from local kem_pk.bin).
 */
shyake_fp_result* shyake_fingerprint(shyake_ctx *ctx,
                                     const char *target_user,
                                     int do_update);

/* --- Local saved mail --- */

typedef struct {
    char *mail_id;
    char *sender;
    char *recipient;
    char *subject;     /* decrypted, NULL if failed */
    int64_t timestamp;
    int size;
} shyake_saved_entry;

typedef struct {
    shyake_saved_entry *entries;
    int count;
} shyake_saved_list;

void shyake_free_saved_list(shyake_saved_list *list);

/*
 * Save encrypted mail JSON to ~/.config/shyake/saved/<id>.json.
 * Returns SHYAKE_OK on success.
 */
shyake_err shyake_save_mail(shyake_ctx *ctx, const char *mail_id);

/*
 * Load and decrypt a saved mail from disk (body included).
 * Returns allocated shyake_mail_detail* on success, NULL on failure.
 */
shyake_mail_detail* shyake_read_saved(shyake_ctx *ctx,
                                       const char *mail_id);

/*
 * Show metadata of a single saved mail (no body decrypt).
 * Returns allocated shyake_mail_detail* on success, NULL on failure.
 */
shyake_mail_detail* shyake_check_saved_one(shyake_ctx *ctx,
                                            const char *mail_id);

/*
 * List all locally saved mail (decrypts subjects).
 * Returns allocated shyake_saved_list* on success, NULL on failure.
 */
shyake_saved_list* shyake_list_saved(shyake_ctx *ctx);

/* --- Local drafts --- */

/*
 * Save an encrypted draft to <config_dir>/drafts/<id>.json.
 * Fields are encrypted to the user's own KEM public key, so no
 * passphrase is needed to save (only to read back).
 * recipient: NULL or "" for a diary entry (no recipient).
 * subject: NULL or "" allowed (unlike send).
 * draft_id: NULL to create a new draft; existing id to overwrite
 *           (created timestamp is preserved, updated is refreshed).
 * out_id: if non-NULL, receives the allocated id string
 *         (caller must free()).
 * Returns SHYAKE_OK on success, SHYAKE_ERR_NOT_FOUND if draft_id
 * does not exist.
 */
shyake_err shyake_save_draft(shyake_ctx *ctx, const char *recipient,
                             const char *subject, const uint8_t *body,
                             size_t body_len, const char *draft_id,
                             char **out_id);

/*
 * List all local drafts sorted by id (decrypts recipient + subject).
 * Returns allocated shyake_saved_list* on success, NULL on failure.
 * Entry recipient is "" for diary drafts.
 */
shyake_saved_list* shyake_list_drafts(shyake_ctx *ctx);

/*
 * Load and decrypt a draft from disk (body included).
 * Returns allocated shyake_mail_detail* on success, NULL on failure.
 */
shyake_mail_detail* shyake_read_draft(shyake_ctx *ctx,
                                      const char *draft_id);

/*
 * Delete a draft by id.
 * Returns SHYAKE_OK, SHYAKE_ERR_NOT_FOUND, or SHYAKE_ERR.
 */
shyake_err shyake_delete_draft(shyake_ctx *ctx, const char *draft_id);

/* --- File encryption / decryption --- */

/*
 * Encrypt a file using own or recipient's KEM public key.
 * recipient: NULL to use own key; otherwise fetches recipient pubkey.
 * out_path: NULL to derive from in_path (appends ".enc").
 * Returns SHYAKE_OK on success.
 */
shyake_err shyake_enc_file(shyake_ctx *ctx,
                            const char *in_path,
                            const char *out_path,
                            const char *recipient);

/*
 * Decrypt a file using own KEM secret key.
 * out_path: NULL to write to stdout.
 * Returns SHYAKE_OK on success.
 */
shyake_err shyake_dec_file(shyake_ctx *ctx,
                            const char *in_path,
                            const char *out_path);

/* --- Client version --- */

typedef struct {
    char *release;            /* latest stable tag, e.g. "v0.1.1" */
    char *pre_release;        /* latest pre-release tag, may be NULL */
    char *release_digest;     /* sha256 hex of this platform's stable
                                 asset, may be NULL */
    char *pre_release_digest; /* same for the preview asset */
} shyake_version_info;

typedef enum {
    SHYAKE_UPDATE_STABLE  = 0,
    SHYAKE_UPDATE_PREVIEW = 1,
} shyake_update_channel;

void shyake_free_version_info(shyake_version_info *v);

/*
 * Semver comparison: >0 if a>b, <0 if a<b, 0 if equal.
 * Handles "vX.Y.Z" and "vX.Y.Z-prerelease" (release > prerelease).
 */
int shyake_version_cmp(const char *a, const char *b);

/*
 * Query version_url for latest client version.
 * Returns allocated shyake_version_info* on success, NULL on failure.
 */
shyake_version_info* shyake_get_latest_version(shyake_ctx *ctx,
                                                const char *version_url);

/*
 * Download and install the latest stable or preview release binary.
 * version_url: URL of the version API endpoint.
 * current_version: running version string (e.g. "v0.1.1").
 * channel: SHYAKE_UPDATE_STABLE or SHYAKE_UPDATE_PREVIEW.
 * Returns SHYAKE_OK on success.
 */
shyake_err shyake_self_update(shyake_ctx *ctx,
                               const char *version_url,
                               const char *current_version,
                               shyake_update_channel channel);

#endif /* SHYAKE_H */
