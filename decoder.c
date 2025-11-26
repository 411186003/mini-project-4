#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_SYMBOLS 256
#define EOF_SYMBOL 255

typedef struct Node {
    int sym;               // -1 表示非葉節點
    struct Node *left;
    struct Node *right;
} Node;

typedef struct {
    int sym;
    char code[128];
} Entry;

int parse_symbol(const char *s) {
    if (strcmp(s, "\\n") == 0) return '\n';
    if (strcmp(s, "\\r") == 0) return '\r';
    if (strcmp(s, "EOF") == 0) return EOF_SYMBOL;
    if (strncmp(s, "0x", 2) == 0) {
        unsigned int val;
        if (sscanf(s, "0x%X", &val) == 1) return val;
    }
    if (strlen(s) == 1) return s[0];
    return -1;
}

Node *new_node(int sym) {
    Node *n = malloc(sizeof(Node));
    n->sym = sym;
    n->left = n->right = NULL;
    return n;
}

void insert_code(Node *root, const char *code, int sym) {
    Node *curr = root;
    for (int i = 0; code[i]; i++) {
        if (code[i] == '0') {
            if (!curr->left) curr->left = new_node(-1);
            curr = curr->left;
        } else if (code[i] == '1') {
            if (!curr->right) curr->right = new_node(-1);
            curr = curr->right;
        } else {
            fprintf(stderr, "invalid code char: %c\n", code[i]);
            exit(1);
        }
    }
    curr->sym = sym;
}

int read_bit(FILE *f) {
    static int bits_left = 0;
    static unsigned char byte = 0;
    if (bits_left == 0) {
        int c = fgetc(f);
        if (c == EOF) return -1;
        byte = (unsigned char)c;
        bits_left = 8;
    }
    int bit = (byte >> 7) & 1;
    byte <<= 1;
    bits_left--;
    return bit;
}

// 取得時間字串
void timestamp(char *buf, size_t sz) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, sz, "%Y-%m-%d %H:%M:%S", tm);
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s output.txt codebook.csv encoded.bin\n", argv[0]);
        return 1;
    }

    const char *out_fn = argv[1];
    const char *cb_fn  = argv[2];
    const char *enc_fn = argv[3];

    char timebuf[32];
    timestamp(timebuf, sizeof(timebuf));
    FILE *flog = fopen("decoder.log", "w");
    if (!flog) { perror("fopen decoder.log"); return 1; }
    fprintf(flog, "%s [INFO] decoder: start \\\n    input_encoded=%s \\\n    input_codebook=%s \\\n    output_file=%s\n",
            timebuf, enc_fn, cb_fn, out_fn);

    // 讀 codebook
    FILE *fcb = fopen(cb_fn, "r");
    if (!fcb) { 
        timestamp(timebuf, sizeof(timebuf));
        fprintf(flog, "%s [ERROR] decoder: cannot_open_codebook\n", timebuf);
        fclose(flog);
        return 1;
    }

    Entry table[MAX_SYMBOLS];
    int entry_count = 0;
    char line[256];
    while (fgets(line, sizeof(line), fcb)) {
        char symbol_str[32], code[128];
        unsigned long count;
        double prob, self_info;
        if (sscanf(line, "\"%[^\"]\",%lu,%lf,\"%[^\"]\",%lf",
                   symbol_str, &count, &prob, code, &self_info) == 5) {
            int s = parse_symbol(symbol_str);
            if (s == -1) continue;
            table[entry_count].sym = s;
            strcpy(table[entry_count].code, code);
            entry_count++;
        }
    }
    fclose(fcb);
    timestamp(timebuf, sizeof(timebuf));
    fprintf(flog, "%s [INFO] decoder: load_codebook entries=%d\n", timebuf, entry_count);

    Node *root = new_node(-1);
    for (int i = 0; i < entry_count; i++) {
        insert_code(root, table[i].code, table[i].sym);
    }

    FILE *fenc = fopen(enc_fn, "rb");
    if (!fenc) { 
        timestamp(timebuf, sizeof(timebuf));
        fprintf(flog, "%s [ERROR] decoder: cannot_open_encoded_file\n", timebuf);
        fclose(flog);
        return 1;
    }
    FILE *fout = fopen(out_fn, "wb");
    if (!fout) { 
        timestamp(timebuf, sizeof(timebuf));
        fprintf(flog, "%s [ERROR] decoder: cannot_open_output_file\n", timebuf);
        fclose(fenc);
        fclose(flog);
        return 1;
    }

    Node *curr = root;
    int bit;
    unsigned long num_decoded = 0;
    unsigned long bit_pos = 0;

    while ((bit = read_bit(fenc)) != -1) {
        bit_pos++;
        curr = (bit == 0) ? curr->left : curr->right;
        if (!curr) {
            timestamp(timebuf, sizeof(timebuf));
            fprintf(flog, "%s [ERROR] decoder: invalid_codeword \\\n    bit_position=%lu \\\n    reason=unexpected_prefix\n",
                    timebuf, bit_pos);
            curr = root;
            continue; // 忽略 padding，繼續
        }
        if (curr->sym != -1) {
            if (curr->sym == EOF_SYMBOL) break;
            fputc(curr->sym, fout);
            num_decoded++;
            curr = root;
        }
    }

    timestamp(timebuf, sizeof(timebuf));
    fprintf(flog, "%s [INFO] decoder: decode_bitstream \\\n    output_file=%s \\\n    num_decoded_symbols=%lu\n",
            timebuf, out_fn, num_decoded);

    timestamp(timebuf, sizeof(timebuf));
    fprintf(flog, "%s [INFO] metrics: summary \\\n    input_encoded=%s \\\n    input_codebook=%s \\\n    output_file=%s \\\n    num_decoded_symbols=%lu \\\n    status=ok\n",
            timebuf, enc_fn, cb_fn, out_fn, num_decoded);

    timestamp(timebuf, sizeof(timebuf));
    fprintf(flog, "%s [INFO] decoder: finish status=ok\n", timebuf);

    fclose(fenc);
    fclose(fout);
    fclose(flog);

    return 0;
}
