#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "logger.h"

#define MAX_SYMBOLS 256
#define MAX_CODE_LEN 128

typedef struct {
    unsigned char sym;
    unsigned long count;
    double prob;
    char code[MAX_CODE_LEN];
    double self_info;
} SymbolEntry;

typedef struct HuffmanNode {
    unsigned char sym;
    unsigned long count;
    struct HuffmanNode *left;
    struct HuffmanNode *right;
} HuffmanNode;

// ----------------- Function prototypes -----------------
void count_symbols(const char *filename, SymbolEntry *symbols, int *num_symbols, unsigned long *total_symbols);
HuffmanNode* build_huffman_tree(SymbolEntry *symbols, int num_symbols);
void generate_code(HuffmanNode *node, char *code, int depth, SymbolEntry *symbols, int num_symbols);
void write_codebook(SymbolEntry *symbols, int num_symbols, const char *filename);
void encode_file(const char *input_file, const char *output_file, SymbolEntry *symbols, int num_symbols);

// ----------------- Main -----------------
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s input.txt codebook.csv encoded.bin\n", argv[0]);
        return 1;
    }

    const char *input_file = argv[1];
    const char *codebook_file = argv[2];
    const char *encoded_file = argv[3];

    SymbolEntry symbols[MAX_SYMBOLS];
    int num_symbols = 0;
    unsigned long total_symbols = 0;

    count_symbols(input_file, symbols, &num_symbols, &total_symbols);

    HuffmanNode *root = build_huffman_tree(symbols, num_symbols);

    char code[MAX_CODE_LEN];
    generate_code(root, code, 0, symbols, num_symbols);

    write_codebook(symbols, num_symbols, codebook_file);

    encode_file(input_file, encoded_file, symbols, num_symbols);

    return 0;
}

// ----------------- Functions -----------------

void count_symbols(const char *filename, SymbolEntry *symbols, int *num_symbols, unsigned long *total_symbols) {
    unsigned long hist[MAX_SYMBOLS] = {0};
    FILE *f = fopen(filename, "rb");
    if (!f) { perror("fopen"); exit(1); }

    int c;
    while ((c = fgetc(f)) != EOF) hist[c]++;
    fclose(f);

    // 添加 EOF symbol，使用 255 表示
    hist[255] = 1;

    int n = 0;
    unsigned long total = 0;
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (hist[i] > 0) {
            symbols[n].sym = (unsigned char)i;
            symbols[n].count = hist[i];
            symbols[n].prob = 0.0;
            symbols[n].code[0] = '\0';
            symbols[n].self_info = 0.0;
            total += hist[i];
            n++;
        }
    }

    for (int i = 0; i < n; i++) {
        symbols[i].prob = (double)symbols[i].count / total;
        symbols[i].self_info = -log2(symbols[i].prob);
    }

    // 遞增排序: primary count, secondary symbol
    for (int i = 0; i < n-1; i++) {
        for (int j = i+1; j < n; j++) {
            if (symbols[i].count > symbols[j].count ||
               (symbols[i].count == symbols[j].count && symbols[i].sym > symbols[j].sym)) {
                SymbolEntry tmp = symbols[i];
                symbols[i] = symbols[j];
                symbols[j] = tmp;
            }
        }
    }

    *num_symbols = n;
    *total_symbols = total;
}

HuffmanNode* build_huffman_tree(SymbolEntry *symbols, int num_symbols) {
    int n = num_symbols;
    HuffmanNode **nodes = malloc(sizeof(HuffmanNode*) * n);
    for (int i = 0; i < n; i++) {
        nodes[i] = malloc(sizeof(HuffmanNode));
        nodes[i]->sym = symbols[i].sym;
        nodes[i]->count = symbols[i].count;
        nodes[i]->left = NULL;
        nodes[i]->right = NULL;
    }

    while (n > 1) {
        int min1 = 0, min2 = 1;
        if (nodes[min1]->count > nodes[min2]->count) { int t = min1; min1 = min2; min2 = t; }
        for (int i = 2; i < n; i++) {
            if (nodes[i]->count < nodes[min1]->count) { min2 = min1; min1 = i; }
            else if (nodes[i]->count < nodes[min2]->count) { min2 = i; }
        }

        HuffmanNode *parent = malloc(sizeof(HuffmanNode));
        parent->count = nodes[min1]->count + nodes[min2]->count;
        parent->left = nodes[min1];
        parent->right = nodes[min2];
        parent->sym = 0;

        if (min1 > min2) { int t = min1; min1 = min2; min2 = t; }
        nodes[min1] = parent;
        nodes[min2] = nodes[n-1];
        n--;
    }

    HuffmanNode *root = nodes[0];
    free(nodes);
    return root;
}

void generate_code(HuffmanNode *node, char *code, int depth, SymbolEntry *symbols, int num_symbols) {
    if (!node) return;
    if (!node->left && !node->right) {
        code[depth] = '\0';
        for (int i = 0; i < num_symbols; i++) {
            if (symbols[i].sym == node->sym) {
                strcpy(symbols[i].code, code);
                return;
            }
        }
    }
    if (node->left) {
        code[depth] = '0';
        generate_code(node->left, code, depth+1, symbols, num_symbols);
    }
    if (node->right) {
        code[depth] = '1';
        generate_code(node->right, code, depth+1, symbols, num_symbols);
    }
}

void write_codebook(SymbolEntry *symbols, int num_symbols, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) { perror("fopen codebook"); exit(1); }

    for (int i = 0; i < num_symbols; i++) {
        const char *sym_str;
        static char tmp[8];

        if (symbols[i].sym == '\n') sym_str = "\\n";
        else if (symbols[i].sym == '\r') sym_str = "\\r";
        else if (symbols[i].sym == 255) sym_str = "EOF";
        else if (symbols[i].sym < 32 || symbols[i].sym > 126) {
            sprintf(tmp, "0x%02X", symbols[i].sym);
            sym_str = tmp;
        } else {
            tmp[0] = symbols[i].sym;
            tmp[1] = '\0';
            sym_str = tmp;
        }

        fprintf(f, "\"%s\",%lu,%.15f,\"%s\",%.15f\n",
                sym_str,
                symbols[i].count,
                symbols[i].prob,
                symbols[i].code,
                symbols[i].self_info);
    }

    fclose(f);
}

void encode_file(const char *input_file, const char *output_file, SymbolEntry *symbols, int num_symbols) {
    FILE *fin = fopen(input_file, "rb");
    FILE *fout = fopen(output_file, "wb");
    if (!fin || !fout) { perror("fopen"); exit(1); }

    int c;
    unsigned char buffer = 0;
    int bits_filled = 0;

    while ((c = fgetc(fin)) != EOF) {
        for (int i = 0; i < num_symbols; i++) {
            if (symbols[i].sym == (unsigned char)c) {
                for (int j = 0; symbols[i].code[j]; j++) {
                    buffer = (buffer << 1) | (symbols[i].code[j]=='1'?1:0);
                    bits_filled++;
                    if (bits_filled == 8) {
                        fputc(buffer, fout);
                        buffer = 0;
                        bits_filled = 0;
                    }
                }
                break;
            }
        }
    }

    // encode EOF
    for (int i = 0; i < num_symbols; i++) {
        if (symbols[i].sym == 255) {
            for (int j = 0; symbols[i].code[j]; j++) {
                buffer = (buffer << 1) | (symbols[i].code[j]=='1'?1:0);
                bits_filled++;
                if (bits_filled == 8) {
                    fputc(buffer, fout);
                    buffer = 0;
                    bits_filled = 0;
                }
            }
            break;
        }
    }

    if (bits_filled > 0) {
        buffer <<= (8 - bits_filled); // 補滿最後一個 byte
        fputc(buffer, fout);
    }

    fclose(fin);
    fclose(fout);
}
