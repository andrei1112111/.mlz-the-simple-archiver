#include <stdio.h>
#include <stdlib.h>

#define uint64 unsigned long long int
#define uchar unsigned char

// https://en.wikipedia.org/wiki/Lempel–Ziv–Welch

// compression is implemented using a prefix tree
// vertex_id - index in the array ('ordinal number' of the phrase in the list of all phrases)
// pVertex - list of possible 'paths'
struct Vertex { // 2064 байт
    uint64 vertex_id;
    uint64 pVertex[257];
};

// adding phrase to the tree
void add_vertex(struct Vertex *root, struct Vertex *arrVertex, uint64 *lVertex, const uchar *key, const uint64 len_key) {
    uint64 i;
    struct Vertex *cur = root;
    for (i = 0; i < len_key; ++i) {
        if (cur->pVertex[key[i] + 1] == 0) {
            arrVertex[*lVertex].vertex_id = *lVertex;
            for (int j = 1; j < 257; ++j) {arrVertex[*lVertex].pVertex[j] = 0;}
            cur->pVertex[key[i] + 1] = *lVertex;
            *lVertex = *lVertex + 1;
        }
        cur = &arrVertex[cur->pVertex[key[i] + 1]];
    }
}

// search for 'phrase' in the tree
// returns the index in the array (the 'ordinal number' of the phrase in the list of all phrases) (the phrase was not found - (0), found - (number + 1))
uint64 find_vertex(struct Vertex root, struct Vertex *arrVertex, const uchar *key, const uint64 len_key) {
    unsigned int j;
    struct Vertex cur = root;
    for (uint64 i = 0; i < len_key; ++i) {
        j = (uchar)key[i];
        if (cur.pVertex[j + 1] == 0) {
            return 0;
        }
        cur = arrVertex[cur.pVertex[j + 1]];
    }
    return (cur.vertex_id);
}

// max_dict_size the maximum number of added vertices N (actual size: (2064)*(512+N))
// max_dict_size = 0 - maximum size is not limited
// start_dict_size: initial tree size N (actual size: (2064)*(512+N)), (2064)*(start_dict_size + 512) - default
// start_dict_size = 0 - default minimum size
// lzw for byte sequences (prefix tree compression)
// maximum input data size - (18,446,744,073,709,551,615 bytes) == (16.7 terabytes)
// returns an array of 'numbers' - codes of all (lossless) phrases
// The result should be freed after
/**
 * @brief Lempel-Ziv-Welch (LZW) compression algorithm.
 * @details This function uses a prefix tree to compress input data.
 * @param[in] input A pointer to the input data.
 * @param[in] size_inp The size of the input data.
 * @param[out] result_len A pointer to the length of the compressed data.
 * @param[out] uSize A pointer to the size of the compressed data in bytes.
 * @param[in] max_dict_size The maximum number of added vertices.
 * @param[in] start_dict_size The initial number of added vertices.
 * @return A pointer to the compressed data, or NULL if memory allocation fails.
 */
uint64 *lzwEncode(const uchar *input, const uint64 size_inp, uint64 *result_len, uchar *uSize, uint64 max_dict_size, uint64 start_dict_size) {
    // определение строки-фразы
    uint64 len_key = 0;
    uint64 key_mem_step = 1024;
    uint64 mem_for_key = 1024;
    uchar *key = malloc(sizeof(uchar) * mem_for_key);
    // определение дерева для хранения фраз
    uint64 lVertex = 257;
    uint64 vert_mem_step;
    uint64 mem_for_vert;
    if (start_dict_size == 0) {
        mem_for_vert = 512;
    } else {
        mem_for_vert = 512 + start_dict_size;
    }
    uint64 MAX_VERTEX_SIZE = 0;
    if (max_dict_size != 0) {
        MAX_VERTEX_SIZE = start_dict_size + 512 + max_dict_size;
        vert_mem_step = MAX_VERTEX_SIZE / 4;
    } else {
        vert_mem_step = mem_for_vert;
    }
    struct Vertex root;
    struct Vertex *arrVertex = malloc(sizeof(struct Vertex) * mem_for_vert);
    // prefilling the root with all possible characters (char range)
    for (int j = 1; j < 257; ++j) { root.pVertex[j] = 0; }
    for (int i = 1; i < 257; ++i) {
        arrVertex[i].vertex_id = i;
        for (int j = 1; j < 257; ++j) { arrVertex[i].pVertex[j] = 0; } // символ = номер символа в pVertex
        root.pVertex[i] = i;
    }
    // result (consumes a lot of memory) //
    uint64 res_len = 0;
    uint64 res_mem_step = 4096;
    uint64 mem_for_res = 4096;
    uint64 *res = malloc(sizeof(uint64) * mem_for_res); // phrase codes, the result of the lzw_encode function

    for (uint64 inp_byte = 0; inp_byte < size_inp; ++inp_byte) { // iterate over input bytes
        if ((len_key + 1) >= mem_for_key) { // there is not enough memory in key to add a new character
            mem_for_key = mem_for_key + key_mem_step;
            key = realloc(key, mem_for_key * sizeof(uchar)); // NOLINT(*-suspicious-realloc-usage)
            if (key == NULL) {
                fprintf(stderr, "Memory allocation error. ->lzw encoding ->for ->key\n");
                free(arrVertex);
                free(key);
                free(res);
                exit(1);
            }
        }
        key[len_key] = input[inp_byte];
        ++len_key; // [key] = key+input[inp_byte]
        if (find_vertex(root, arrVertex, key, len_key) == 0) { // check key input inp_byte not in dict
            if ((lVertex + 1) > mem_for_vert) { // memory under the tree is over
                mem_for_vert = mem_for_vert + vert_mem_step;
                arrVertex = (struct Vertex *) realloc(arrVertex, mem_for_vert * sizeof(struct Vertex)); // NOLINT(*-suspicious-realloc-usage)
                if (arrVertex == NULL) {
                    fprintf(stderr, "Memory allocation error. ->lzw encoding ->for ->arrVertex\n");
                    free(arrVertex);
                    free(key);
                    free(res);
                    exit(1);
                }
            }
            if ((res_len + 1) > mem_for_res) { // the memory for the result has run out
                mem_for_res = mem_for_res + res_mem_step;
                res = realloc(res, mem_for_res * sizeof(uint64)); // NOLINT(*-suspicious-realloc-usage)
                if (res == NULL) {
                    fprintf(stderr, "Memory allocation error. ->lzw encoding ->for ->res\n");
                    free(arrVertex);
                    free(key);
                    free(res);
                    exit(1);
                }
            }
            // result.append(dictionary[key])
            res[res_len] = find_vertex(root, arrVertex, key, (len_key - 1)) - 1;
            ++res_len;
            // dictionary.append(key+input[inp_byte])
            if (MAX_VERTEX_SIZE == 0 || mem_for_vert < MAX_VERTEX_SIZE) {
                add_vertex(&root, arrVertex, &lVertex, key, len_key);
            }
            // key = input[inp_byte]
            key[0] = input[inp_byte];
            len_key = 1;
        }
    }
    if (len_key != 0) {
        if ((res_len + 1) > mem_for_res) { // the memory for the result has run out
            mem_for_res = mem_for_res + res_mem_step;
            res = realloc(res, mem_for_res * sizeof(uint64)); // NOLINT(*-suspicious-realloc-usage)
            if (res == NULL) {
                fprintf(stderr, "Memory allocation error. ->lzw encoding ->main ->res\n");
                free(arrVertex);
                free(key);
                free(res);
                exit(1);
            }
        }
        // result.append(dictionary[key])
        res[res_len] = (find_vertex(root, arrVertex, key, len_key) - 1);
        ++res_len;
    }
    *result_len = res_len;
    uint64 maxS = 18446744073709551614;
    if (lVertex >= maxS) {
        printf("ПЕРЕПОЛНЕНИЕ ДЛИННЫ ВЕРТЕКСА");
        free(arrVertex); free(key); free(res);
        return NULL;
    }
    if (lVertex < maxS) {
        *uSize = 8;
    }
    if (lVertex < 72057594037927934) {
        *uSize = 7;
    }
    if (lVertex < 281474976710654) {
        *uSize = 6;
    }
    if (lVertex < 1099511627774) {
        *uSize = 5;
    }
    if (lVertex < 4294967294) {
        *uSize = 4;
    }
    if (lVertex < 16777214) {
        *uSize = 3;
    }
    if (lVertex < 65534) {
        *uSize = 2;
    }
    free(arrVertex); free(key);
    return res;
}

// String data type (len of string <= 65535)
typedef struct String {
    unsigned short int len;
    uchar *str;
} String;


/**
 * @brief lzw for byte sequences (Decoding using String Array)
 * @details accepts a sequence of code numbers and returns the original sequence (without loss)
 * @param[in] input the input file pointer
 * @param[in] uInpSize the size of the input data
 * @param[in] size_inp the size of the input data in bytes
 * @param[in,out] output the output file pointer
 * @return none
 * @note The result should be freed after
 */
void lzwDecode(FILE *input, int uInpSize, const uint64 size_inp, FILE *output) {
    // defining a string key
    uint64 len_key; uint64 key_mem_step = 4096; uint64 mem_for_key = 4096;
    uchar *key = malloc(sizeof(uchar ) * mem_for_key);
    // defining the second string key
    uint64 len_entry; uint64 entry_mem_step = 4096; uint64 mem_for_entry = 4096;
    uchar *entry = malloc(sizeof(uchar ) * mem_for_entry);
    // the dictionary consists of the length (the first byte for now) and the sequence itself further
    uint64 dict_len = 256; uint64 dict_mem_step = 4096; uint64 mem_for_dict = 4096;
    String *dict = malloc(sizeof(String) * mem_for_dict); // definition of dictionary
    // filling the dictionary with all possible meanings
    for (int i = 0; i < 256; ++i) { dict[i].str = malloc(sizeof(uchar ) * 2) ; dict[i].len = 1; dict[i].str[0] = (uchar )i;}
    uint64 current = 0;
    fread(&current, uInpSize, 1, input);
    key[0] = (uchar )current; len_key = 1;
    uchar hash = (uchar )current;
    fwrite(&hash, sizeof(uchar), 1, output);
    for (uint64 inp_int = 1; inp_int < size_inp; ++inp_int) { // pass through int of input
        current = 0;
        fread(&current, uInpSize, 1, input);
        if (current < dict_len) { // input[inp_int] in dict
            if ((dict[current].len) >= mem_for_entry) { // there is not enough memory in entry
                while ((dict[current].len) >= mem_for_entry) {
                    mem_for_entry = mem_for_entry + entry_mem_step;
                }
                entry = realloc(entry, mem_for_entry * sizeof(uchar )); // NOLINT(*-suspicious-realloc-usage)
                if (entry == NULL) {
                    for (uint64 i = 0; i < dict_len; ++i) {free(dict[i].str);}
                    free(key); free(dict);
                    fprintf(stderr, "Memory allocation error. ->lzw decoding -> entry\n");
                    exit(1);
                }
            }
            len_entry = dict[current].len;
            for (uint64 i = 0; i < len_entry; ++i) {entry[i] = dict[current].str[i];}
        } else if (current == dict_len || current == dict_len + 1) { // input[inp_int] not in dict
            if ((len_key + 1) >= mem_for_entry) { // there is not enough memory in entry
                mem_for_entry = mem_for_entry + entry_mem_step;
                entry = realloc(entry, mem_for_entry * sizeof(uchar )); // NOLINT(*-suspicious-realloc-usage)
                if (entry == NULL) {
                    for (uint64 i = 0; i < dict_len; ++i) {free(dict[i].str);}
                    free(key); free(dict);
                    fprintf(stderr, "Memory allocation error. ->lzw decoding -> entry\n");
                    exit(1);
                }
            }
            // entry = key + key[0]
            len_entry = len_key;
            for (uint64 i = 0; i < len_key; ++i) {entry[i] = key[i];}
            entry[len_entry] = key[0];
            ++len_entry;
        } else {
            printf("bad code %llu %llu ", current, dict_len);
            for (uint64 i = 0; i < dict_len; ++i) {free(dict[i].str);}
            free(entry); free(key); free(dict);
            return ;
        }
        for (uint64 i = 0; i < len_entry; ++i) { // res += entry
            fwrite(&entry[i], sizeof(uchar), 1, output);
        }
        // there is not enough space in the dictionary for +1 entry
        if ((dict_len + 1) > mem_for_dict) { // memory for the dictionary has run out
            mem_for_dict = mem_for_dict + dict_mem_step;
            dict = (String *) realloc(dict, mem_for_dict * sizeof(String)); // NOLINT(*-suspicious-realloc-usage)
            if (dict == NULL) {
                fprintf(stderr, "Memory allocation error. ->lzw decoding -> dict\n");
                free(entry); free(key);
                exit(1);
            }
        }
//        dictionary[dict_size] = w + entry[0]
        dict[dict_len].str = malloc(sizeof(uchar ) * (len_key + 1));
        dict[dict_len].len = (unsigned short int)(len_key + 1);
        for (uint64 i = 0; i < len_key; ++i) {dict[dict_len].str[i] = key[i];}
        dict[dict_len].str[len_key] = entry[0];
        ++dict_len;

        // there is not enough memory for entry in key
        if (len_entry >= mem_for_key) {
            while (len_entry >= mem_for_key) {mem_for_key = mem_for_key + key_mem_step;}
            key = realloc(key, mem_for_key * sizeof(uchar )); // NOLINT(*-suspicious-realloc-usage)
            if (key == NULL) {
                for (uint64 i = 0; i < dict_len; ++i) {free(dict[i].str);}
                free(entry); free(dict);
                fprintf(stderr, "Memory allocation error. ->lzw decoding -> key\n");
                exit(1);
            }
        }
        // key = entry
        for (uint64 i = 0; i < len_entry; ++i) {key[i] = entry[i];}
        len_key = len_entry;
    }
    for (uint64 i = 0; i < dict_len; ++i) {free(dict[i].str);}
    free(entry); free(key); free(dict);
}
