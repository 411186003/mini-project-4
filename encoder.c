#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "logger.h"

#define MAX_SYMBOLS 256
#define MAX_CODE_LEN 64

typedef struct {
    unsigned char symbol;
    unsigned long count;
    double probability;
    char codeword[MAX_CODE_LEN];
} symbol_t;

/* Huffman tree 節點 */
typedef struct huff_node {
    unsigned char symbol;
    unsigned long count;
    struct huff_node *left;
    struct huff_node *right;
} huff_node_t;

/* 優先佇列節點 */
typedef struct {
    huff_node_t *node;
} pq_t;

/* function prototypes */
void count_symbols(const char *filename, symbol_t symbols[], int *unique_symbols, unsigned long *total_symbols);
huff_node_t *build_huffman_tree(symbol_t symbols[], int unique_symbols);
void assign_codewords(huff_node_t *root, char *code, int depth, symbol_t symbols[]);
void generate_codebook_csv(const char *filename, symbol_t symbols[], int unique_symbols);
void encode_to_bitstream(const char *input_file, const char *output_file, symbol_t symbols[], int unique_symbols);

/* 優先佇列操作 */
void pq_push(pq_t *pq, huff_node_t *node, int *size);
huff_node_t *pq_pop(pq_t *pq, int *size);

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s input.txt codebook.csv encoded.bin\n", argv[0]);
        return 1;
    }

    const char *input_file   = argv[1];
    const char *codebook_file= argv[2];
    const char *encoded_file = argv[3];

    log_init(NULL, NULL);  // stdout/stderr
    log_info("encoder", "start input_file=%s", input_file);

    symbol_t symbols[MAX_SYMBOLS] = {0};
    int unique_symbols = 0;
    unsigned long total_symbols = 0;

    count_symbols(input_file, symbols, &unique_symbols, &total_symbols);
    log_info("encoder", "count_symbols done total_symbols=%lu unique_symbols=%d", total_symbols, unique_symbols);

    huff_node_t *root = build_huffman_tree(symbols, unique_symbols);
    log_info("encoder", "build_huffman_tree done");

    char code[MAX_CODE_LEN];
    assign_codewords(root, code, 0, symbols);
    log_info("encoder", "assign_codewords done");

    generate_codebook_csv(codebook_file, symbols, unique_symbols);
    log_info("encoder", "generate_codebook output_codebook=%s", codebook_file);

    encode_to_bitstream(input_file, encoded_file, symbols, unique_symbols);
    log_info("encoder", "encode_to_bitstream output_encoded=%s", encoded_file);

    log_info("metrics", "summary input_file=%s output_codebook=%s output_encoded=%s num_symbols=%lu unique_symbols=%d",
             input_file, codebook_file, encoded_file, total_symbols, unique_symbols);

    log_info("encoder", "finish status=ok");
    return 0;
}

/* 計算 histogram */
void count_symbols(const char *filename, symbol_t symbols[], int *unique_symbols, unsigned long *total_symbols) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        log_error("encoder", "cannot_open_file filename=%s", filename);
        exit(1);
    }

    int c;
    while ((c = fgetc(fp)) != EOF) {
        unsigned char uc = (unsigned char)c;
        if (symbols[uc].count == 0) {
            symbols[uc].symbol = uc;
            (*unique_symbols)++;
        }
        symbols[uc].count++;
        (*total_symbols)++;
    }

    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (symbols[i].count > 0) {
            symbols[i].probability = (double)symbols[i].count / *total_symbols;
        }
    }
    fclose(fp);
}

/* 優先佇列操作 */
void pq_push(pq_t *pq, huff_node_t *node, int *size) {
    int i = (*size)++;
    while (i > 0 && pq[i-1].node->count > node->count) {
        pq[i] = pq[i-1];
        i--;
    }
    pq[i].node = node;
}

huff_node_t *pq_pop(pq_t *pq, int *size) {
    if (*size <= 0) return NULL;
    huff_node_t *node = pq[0].node;
    for (int i = 1; i < *size; i++) pq[i-1] = pq[i];
    (*size)--;
    return node;
}

/* 建 Huffman tree */
huff_node_t *build_huffman_tree(symbol_t symbols[], int unique_symbols) {
    pq_t pq[MAX_SYMBOLS];
    int pq_size = 0;

    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (symbols[i].count > 0) {
            huff_node_t *node = malloc(sizeof(huff_node_t));
            node->symbol = symbols[i].symbol;
            node->count = symbols[i].count;
            node->left = node->right = NULL;
            pq_push(pq, node, &pq_size);
        }
    }

    while (pq_size > 1) {
        huff_node_t *a = pq_pop(pq, &pq_size);
        huff_node_t *b = pq_pop(pq, &pq_size);
        huff_node_t *parent = malloc(sizeof(huff_node_t));
        parent->symbol = 0;
        parent->count = a->count + b->count;
        parent->left = a;   // left = 1
        parent->right = b;  // right = 0
        pq_push(pq, parent, &pq_size);
    }

    return pq_pop(pq, &pq_size); // root
}

/* 指派 codeword */
void assign_codewords(huff_node_t *root, char *code, int depth, symbol_t symbols[]) {
    if (!root) return;
    if (!root->left && !root->right) {
        code[depth] = '\0';
        strcpy(symbols[root->symbol].codeword, code);
        return;
    }
    code[depth] = '1';
    assign_codewords(root->left, code, depth+1, symbols);
    code[depth] = '0';
    assign_codewords(root->right, code, depth+1, symbols);
}

/* 產生 codebook.csv */
void generate_codebook_csv(const char *filename, symbol_t symbols[], int unique_symbols) {
    symbol_t valid_symbols[MAX_SYMBOLS];
    int n = 0;
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (symbols[i].count > 0) {
            valid_symbols[n++] = symbols[i];
        }
    }

    /* 排序：primary count, secondary symbol */
    for (int i = 0; i < n-1; i++) {
        for (int j = i+1; j < n; j++) {
            if (valid_symbols[i].count > valid_symbols[j].count ||
                (valid_symbols[i].count == valid_symbols[j].count &&
                 valid_symbols[i].symbol > valid_symbols[j].symbol)) {
                symbol_t tmp = valid_symbols[i];
                valid_symbols[i] = valid_symbols[j];
                valid_symbols[j] = tmp;
            }
        }
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        log_error("encoder", "cannot_open_codebook filename=%s", filename);
        exit(1);
    }

    for (int i = 0; i < n; i++) {
        char sym_buf[8];
        if (valid_symbols[i].symbol == '\n') strcpy(sym_buf, "\\n");
        else if (valid_symbols[i].symbol == '\r') strcpy(sym_buf, "\\r");
        else if (valid_symbols[i].symbol == '"') strcpy(sym_buf, "\\\"");
        else if (valid_symbols[i].symbol == ',') strcpy(sym_buf, "\\,");
        else if (valid_symbols[i].symbol == '\\') strcpy(sym_buf, "\\\\");
        else sprintf(sym_buf, "%c", valid_symbols[i].symbol);

        fprintf(fp, "\"%s\",%lu,%.15f,%s,%.15f\n",
                sym_buf,
                valid_symbols[i].count,
                valid_symbols[i].probability,
                valid_symbols[i].codeword,
                -log2(valid_symbols[i].probability));
    }

    fclose(fp);
}

/* encode bitstream */
void encode_to_bitstream(const char *input_file, const char *output_file, symbol_t symbols[], int unique_symbols) {
    FILE *fp_in = fopen(input_file, "r");
    FILE *fp_out= fopen(output_file, "wb");
    if (!fp_in || !fp_out) {
        log_error("encoder", "cannot_open_file for encoding");
        exit(1);
    }

    unsigned char buffer = 0;
    int bit_count = 0;
    int c;

    while ((c = fgetc(fp_in)) != EOF) {
        unsigned char uc = (unsigned char)c;
        char *code = symbols[uc].codeword;
        for (int i = 0; code[i]; i++) {
            buffer <<= 1;
            if (code[i] == '1') buffer |= 1;
            bit_count++;
            if (bit_count == 8) {
                fputc(buffer, fp_out);
                bit_count = 0;
                buffer = 0;
            }
        }
    }

    if (bit_count > 0) {
        buffer <<= (8 - bit_count); // pad 0
        fputc(buffer, fp_out);
    }

    fclose(fp_in);
    fclose(fp_out);
}
