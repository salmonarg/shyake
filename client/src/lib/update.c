#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>
#include "vendor/cJSON/cJSON.h"
#include "lib_internal.h"
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

void
shyake_free_version_info(shyake_version_info *v)
{
    if (!v) return;
    free(v->release);
    free(v->pre_release);
    free(v);
}

/* parse "vX.Y.Z" or "vX.Y.Z-anything" into components */
static void
parse_ver(const char *s, int *maj, int *min, int *pat, int *has_pre)
{
    *maj = *min = *pat = *has_pre = 0;
    if (!s || !*s) return;
    const char *p = (*s == 'v') ? s + 1 : s;
    *maj = atoi(p);
    p = strchr(p, '.');
    if (!p) return;
    p++;
    *min = atoi(p);
    p = strchr(p, '.');
    if (!p) return;
    p++;
    *pat = atoi(p);
    *has_pre = (strchr(p, '-') != NULL);
}

int
shyake_version_cmp(const char *a, const char *b)
{
    int a_maj, a_min, a_pat, a_pre;
    int b_maj, b_min, b_pat, b_pre;
    parse_ver(a, &a_maj, &a_min, &a_pat, &a_pre);
    parse_ver(b, &b_maj, &b_min, &b_pat, &b_pre);

    if (a_maj != b_maj) return a_maj - b_maj;
    if (a_min != b_min) return a_min - b_min;
    if (a_pat != b_pat) return a_pat - b_pat;
    /* same base: release (no pre) > prerelease */
    if (a_pre != b_pre) return a_pre ? -1 : 1;
    return 0;
}

shyake_version_info*
shyake_get_latest_version(shyake_ctx *ctx, const char *version_url)
{
    if (!ctx || !version_url) return NULL;

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    if (ctx->debug)
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    struct curl_response resp = { .data = malloc(1), .size = 0 };
    resp.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, version_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    shyake_version_info *info = NULL;

    if (res == CURLE_OK) {
        long http_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code == 200) {
            cJSON *json = cJSON_Parse(resp.data);
            if (json) {
                cJSON *rel = cJSON_GetObjectItem(json, "release");
                cJSON *pre = cJSON_GetObjectItem(json, "pre_release");

                info = calloc(1, sizeof(shyake_version_info));
                if (rel && cJSON_IsString(rel))
                    info->release = strdup(rel->valuestring);
                if (pre && cJSON_IsString(pre))
                    info->pre_release = strdup(pre->valuestring);

                cJSON_Delete(json);
            }
        }
    }

    free(resp.data);
    curl_easy_cleanup(curl);
    return info;
}

/* download a URL to a tmp file, return allocated path or NULL */
static char*
download_to_tmp(shyake_ctx *ctx, const char *download_url,
                const char *filename)
{
    char *tmp_path = malloc(256);
    snprintf(tmp_path, 256, "/tmp/%s", filename);

    CURL *curl = curl_easy_init();
    if (!curl) { free(tmp_path); return NULL; }

    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        curl_easy_cleanup(curl);
        free(tmp_path);
        return NULL;
    }

    if (ctx->debug)
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    curl_easy_setopt(curl, CURLOPT_URL, download_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    CURLcode res = curl_easy_perform(curl);
    fclose(f);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        remove(tmp_path);
        free(tmp_path);
        return NULL;
    }
    return tmp_path;
}

shyake_err
shyake_self_update(shyake_ctx *ctx, const char *version_url,
                   const char *current_version,
                   shyake_update_channel channel)
{
    if (!ctx) return SHYAKE_ERR;

    shyake_version_info *info = shyake_get_latest_version(ctx, version_url);
    if (!info) {
        fprintf(stderr, "Failed to fetch version info.\n");
        return SHYAKE_ERR_NETWORK;
    }

    if (!info->release) {
        shyake_free_version_info(info);
        fprintf(stderr, "No stable release available.\n");
        return SHYAKE_ERR;
    }

    const char *target = (channel == SHYAKE_UPDATE_PREVIEW)
        ? info->pre_release : info->release;
    const char *channel_name = (channel == SHYAKE_UPDATE_PREVIEW)
        ? "preview" : "stable";

    if (!target) {
        fprintf(stderr, "No %s release available.\n", channel_name);
        shyake_free_version_info(info);
        return SHYAKE_ERR;
    }

    /* reject preview that is not newer than stable */
    if (channel == SHYAKE_UPDATE_PREVIEW &&
        shyake_version_cmp(info->pre_release, info->release) <= 0) {
        fprintf(stderr,
                "No preview release available newer than stable.\n");
        shyake_free_version_info(info);
        return SHYAKE_ERR;
    }

    /* already on the requested target */
    if (current_version && strcmp(current_version, target) == 0) {
        printf("Already on the latest %s release.\n", channel_name);
        shyake_free_version_info(info);
        return SHYAKE_OK;
    }

    /* detect OS/arch for asset filename */
#if defined(__APPLE__) && defined(__aarch64__)
    const char *asset = "shyake-macos-arm64.tar.gz";
#elif defined(__APPLE__)
    const char *asset = "shyake-macos-x86_64.tar.gz";
#elif defined(__linux__) && defined(__aarch64__)
    const char *asset = "shyake-linux-arm64.tar.gz";
#else
    const char *asset = "shyake-linux-x86_64.tar.gz";
#endif

    char dl_url[512], sha_url[512];
    snprintf(dl_url, sizeof(dl_url),
             "https://github.com/salmonization/shyake/releases/download"
             "/%s/%s", target, asset);
    snprintf(sha_url, sizeof(sha_url),
             "https://github.com/salmonization/shyake/releases/download"
             "/%s/sha256sums.txt", target);

    fprintf(stderr, "Downloading %s %s...\n", target, asset);

    char sha_filename[64];
    snprintf(sha_filename, sizeof(sha_filename), "shyake-sha256sums.txt");
    char *sha_path = download_to_tmp(ctx, sha_url, sha_filename);
    if (!sha_path) {
        fprintf(stderr, "Failed to download checksum file.\n");
        shyake_free_version_info(info);
        return SHYAKE_ERR_NETWORK;
    }

    char *tar_path = download_to_tmp(ctx, dl_url, asset);
    if (!tar_path) {
        fprintf(stderr, "Failed to download release archive.\n");
        remove(sha_path); free(sha_path);
        shyake_free_version_info(info);
        return SHYAKE_ERR_NETWORK;
    }

    /* verify sha256 */
    char sha_cmd[512];
    snprintf(sha_cmd, sizeof(sha_cmd),
             "cd /tmp && grep '%s' '%s' | sha256sum --check --quiet 2>/dev/null",
             asset, sha_path);
    int sha_ret = system(sha_cmd);
    if (sha_ret != 0) {
        fprintf(stderr, "SHA-256 verification failed. Aborting.\n");
        remove(sha_path); remove(tar_path);
        free(sha_path); free(tar_path);
        shyake_free_version_info(info);
        return SHYAKE_ERR_CRYPTO;
    }

    /* find current binary path */
    char self_path[512] = {0};
#if defined(__linux__)
    readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
#elif defined(__APPLE__)
    uint32_t sz = sizeof(self_path);
    _NSGetExecutablePath(self_path, &sz);
#endif

    if (self_path[0] == '\0') {
        FILE *wp = popen("which shyake", "r");
        if (wp) { fgets(self_path, sizeof(self_path), wp); pclose(wp); }
        size_t l = strlen(self_path);
        if (l > 0 && self_path[l-1] == '\n') self_path[l-1] = '\0';
    }

    if (self_path[0] == '\0') {
        fprintf(stderr, "Cannot determine shyake binary path.\n");
        remove(sha_path); remove(tar_path);
        free(sha_path); free(tar_path);
        shyake_free_version_info(info);
        return SHYAKE_ERR;
    }

    char extract_cmd[1024];
    snprintf(extract_cmd, sizeof(extract_cmd),
             "tar xzf '%s' -C /tmp && cp /tmp/shyake '%s' && chmod 755 '%s'",
             tar_path, self_path, self_path);
    int ext_ret = system(extract_cmd);

    char installed_ver[64];
    snprintf(installed_ver, sizeof(installed_ver), "%s", target);

    remove(sha_path); remove(tar_path);
    free(sha_path); free(tar_path);
    shyake_free_version_info(info);

    if (ext_ret != 0) {
        fprintf(stderr, "Installation failed.\n");
        return SHYAKE_ERR;
    }

    printf("Successfully updated to %s.\n", installed_ver);
    return SHYAKE_OK;
}
