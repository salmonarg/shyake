#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include "prompt.h"
#include "shyake.h"

int
read_passphrase(const char *prompt_str, char *buf, size_t buflen)
{
    struct termios old, noecho;
    int saved = 0;

    fprintf(stderr, "%s", prompt_str);
    fflush(stderr);

    if (tcgetattr(STDIN_FILENO, &old) == 0) {
        noecho = old;
        noecho.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &noecho);
        saved = 1;
    }

    char *result = fgets(buf, (int)buflen, stdin);

    if (saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);
        fputs("\n", stderr);
        fflush(stderr);
    }

    if (!result) { buf[0] = '\0'; return -1; }

    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';
    return 0;
}

int
sk_file_is_encrypted(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char magic[4];
    int n = (int)fread(magic, 1, 4, f);
    fclose(f);
    return n == 4 && magic[0] == 'S' && magic[1] == 'H'
                  && magic[2] == 'Y' && magic[3] == 'K';
}

int
prompt_passphrase(shyake_ctx *ctx, const char *config_dir)
{
    char sk_path[512];
    snprintf(sk_path, sizeof(sk_path), "%s/kem_sk.bin", config_dir);
    if (!sk_file_is_encrypted(sk_path))
        return 0;

    char prompt_str[600];
    snprintf(prompt_str, sizeof(prompt_str),
             "Enter passphrase for key '%s': ", sk_path);

    char buf[512];
    if (read_passphrase(prompt_str, buf, sizeof(buf)) != 0) {
        memset(buf, 0, sizeof(buf));
        return -1;
    }
    shyake_set_passphrase(ctx, buf);
    memset(buf, 0, sizeof(buf));
    return 0;
}

int
prompt_new_passphrase(shyake_ctx *ctx, const char *config_dir)
{
    char sk_path[512];
    snprintf(sk_path, sizeof(sk_path), "%s/kem_sk.bin", config_dir);

    char prompt_str[600];
    snprintf(prompt_str, sizeof(prompt_str),
             "Enter passphrase for key '%s' (empty for no passphrase): ",
             sk_path);

    char pp1[512], pp2[512];
    if (read_passphrase(prompt_str, pp1, sizeof(pp1)) != 0) {
        memset(pp1, 0, sizeof(pp1));
        return -1;
    }

    if (pp1[0] == '\0') {
        shyake_set_new_passphrase(ctx, "");
        return 0;
    }

    if (read_passphrase("Enter same passphrase again: ", pp2, sizeof(pp2)) != 0) {
        memset(pp1, 0, sizeof(pp1));
        memset(pp2, 0, sizeof(pp2));
        return -1;
    }

    if (strcmp(pp1, pp2) != 0) {
        fprintf(stderr, "Passphrases do not match.\n");
        memset(pp1, 0, sizeof(pp1));
        memset(pp2, 0, sizeof(pp2));
        return -1;
    }

    shyake_set_new_passphrase(ctx, pp1);
    memset(pp1, 0, sizeof(pp1));
    memset(pp2, 0, sizeof(pp2));
    return 0;
}
