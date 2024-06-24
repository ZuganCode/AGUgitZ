#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <assert.h>
#include <math.h>

bool trunc_file(FILE* file, int64_t length) {
    fseek(file, 0, SEEK_SET);
    return 0 == _chsize_s(_fileno(file), length);
}

int int_min(int a, int b) {
    return a > b ? b : a;
}
int64_t int64_min(int64_t a, int64_t b) {
    return a > b ? b : a;
}

typedef struct s_node {
    int value;
    int64_t length;
    struct s_node* left;
    struct s_node* right;
} Node;

#define HEAP_COMPARATOR(a, b, dir) ( \
    (a)->length == (b)->length ?    \
        ((a)->value > (b)->value) : \
        ((a)->length < (b)->length) \
)

typedef struct s_heap {
    Node** arr;
    int length;
    int size;
} Heap;

void pretty_bytes(char* buf, int64_t bytes) {
    const char* suffixes[7];
    suffixes[0] = "B";
    suffixes[1] = "KB";
    suffixes[2] = "MB";
    suffixes[3] = "GB";
    suffixes[4] = "TB";
    suffixes[5] = "PB";
    suffixes[6] = "EB";
    int64_t s = 0; // which suffix to use
    double count = (double)bytes;
    while (count >= 1024 && s < 7)
    {
        s++;
        count /= 1024;
    }
    if (count - floor(count) == 0.0)
        sprintf(buf, "%d %s", (int)count, suffixes[s]);
    else
        sprintf(buf, "%.1f %s", count, suffixes[s]);
}

typedef struct s_huffman_code {
    int length;
    char code[256];
} HuffmanCode;

typedef struct s_huffman_coder {
    int64_t counts_table[256];
    unsigned char* _read_buffer;
    int _read_buffer_size;
    double read_progress;
    Heap* build_heap;
    Node* symbols_nodes;
    Node* intermediate_nodes;
    HuffmanCode* codes;
    Node* root_node;
} HuffmanCoder;

char* get_file_name_from_path(char* path)
{
    if (path == NULL)
        return NULL;

    char* pFileName = path;
    for (char* pCur = path; *pCur != '\0'; pCur++)
    {
        if (*pCur == '/' || *pCur == '\\')
            pFileName = pCur + 1;
    }

    return pFileName;
}

typedef struct s_archive {
    char*file_name;
    int file_count; // count of files
    char** included_files;
    int64_t compressing_current;
    int64_t compressing_total;
    int64_t decompressing_current;
    int64_t decompressing_total;
    bool all_work_finished;
    char** processed_files;
    HuffmanCoder* current_coder;
    FILE* archive_stream;
    uint8_t archive_hash[16]; // setting by read_archive_header()
    int archive_files_count; //  setting by read_archive_header()
    double time_spent;
    char*writing_file_name; //      make available to delete corrupted file then ERROR
    FILE* writing_file_stream; //   make available to delete corrupted file then ERROR
    int64_t last_safe_eof; //       pos of last complete written file (used for truc file if ERROR)
} Archive;

typedef struct s_archive_file {
    char*file_name;
    int file_id;
    int64_t compressed_file_size;
    int64_t original_file_size;
    time_t add_date;
    uint8_t file_hash[16];
} ArchiveFile;


int heap_left(int i) {
    return 2 * i + 1;
}
int heap_right(int i) {
    return 2 * i + 2;
}
int heap_parent(int i) {
    return (i - 1) / 2;
}
void swap_Node(Node** a, Node** b) {
    Node* tmp = *a;
    *a = *b;
    *b = tmp;
}

Heap* heap_create(int size) {
    Heap* h = calloc(1, sizeof(Heap));
    assert(h);
    h->arr = calloc(size, sizeof(Node*));
    h->size = size;
    return h;
}
void heap_siftup(Heap* h, int i) {
    if (i == 0) 
    {
        return;
    }
    if (HEAP_COMPARATOR(h->arr[i], h->arr[heap_parent(i)], 1)) 
    {
        swap_Node(&h->arr[i], &h->arr[heap_parent(i)]);
        heap_siftup(h, heap_parent(i));
    }
}
void heap_siftdown(Heap* h, int i) {
    int l = heap_left(i);
    int r = heap_right(i);

    int smallest = i;
    if (l < h->length && l < h->size && HEAP_COMPARATOR(h->arr[l], h->arr[i], 0)) {
        smallest = l;
    }
    if (r < h->length && r < h->size && HEAP_COMPARATOR(h->arr[r], h->arr[smallest], 0)) {
        smallest = r;
    }
    if (smallest != i) {
        swap_Node(&h->arr[i], &h->arr[smallest]);
        heap_siftdown(h, smallest);
    }
}

void heap_push(Heap* h, Node* n) {
    if (h->length >= h->size) {
        assert(0 && "heap is full");
    }
    h->arr[h->length] = n;
    h->length++;
    heap_siftup(h, h->length - 1);
}
Node* heap_pop(Heap* h) {
    Node* res = h->arr[0];
    h->arr[0] = h->arr[h->length - 1];
    Node* empty = { 0 };
    h->arr[h->length - 1] = empty;
    h->length--;
    heap_siftdown(h, 0);
    return res;
}
int heap_is_empty(Heap* h) {
    return h->length == 0;
}
void heap_clear(Heap* h) {
    h->length = 0;
}
int heap_count(Heap* h) {
    return h->length;
}
void heap_free(Heap* h) {
    free(h->arr);
    free(h);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

int node_length(Node* node) {
    if (node == NULL)
        return 0;
    return node->length;
}

Node* l_build_tree(HuffmanCoder* coder) {
    assert(coder->intermediate_nodes == NULL);
    Node* sub_nodes = calloc(256, sizeof(Node));
    coder->intermediate_nodes = sub_nodes;
    Heap* heap = heap_create(256 * 2);

    for (int i = 0; i < 256; ++i) {
        if (coder->symbols_nodes[i].length > 0) {
            
            heap_push(heap, &coder->symbols_nodes[i]);
        }
    }

    int sub_node_index = 0;
    while (heap_count(heap) > 1) {
        Node* a = heap_pop(heap);
        Node* b = heap_pop(heap);

        Node united;
        united.right = a;
        united.left = b;
        united.length = node_length(a) + node_length(b);
        united.value = -1;
        sub_nodes[sub_node_index++] = united;

        heap_push(heap, &sub_nodes[sub_node_index - 1]);
    }

    Node* root = heap_pop(heap);
    heap_free(heap);
    return root;
}

void l_get_code_rec(HuffmanCode* codes, Node* current, char* way, int way_i) {
    if (current->value != -1) {
        if (way_i != 0) {
            //printf("> %d %s\n", current->value, way);
            strcpy(codes[current->value].code, way);
            codes[current->value].length = way_i;
        }
        else {
            codes[current->value].code[0] = '0';
            codes[current->value].length = 1;
        }
    }
    if (current->left) {
        way[way_i] = '0';
        l_get_code_rec(codes, current->left, way, way_i + 1);
        way[way_i] = '\0';
    }
    if (current->right) {
        way[way_i] = '1';
        l_get_code_rec(codes, current->right, way, way_i + 1);
        way[way_i] = '\0';
    }
}

void l_generate_symbols_nodes(HuffmanCoder* coder) {
    assert(coder->symbols_nodes == NULL);

    Node* nodes = calloc(256, sizeof(Node));

    for (int i = 0; i < 256; ++i) {
        nodes[i].value = i;
        nodes[i].length = coder->counts_table[i];
    }

    coder->symbols_nodes = nodes;
}

HuffmanCode* l_get_codes(Node* root, int n) {
    HuffmanCode* codes = calloc(n, sizeof(HuffmanCode));
    char way[256] = { 0 };
    l_get_code_rec(codes, root, way, 0);
    return codes;
}


HuffmanCoder* huffman_coder_create() {
    HuffmanCoder* c = calloc(1, sizeof(HuffmanCoder));
    assert(c);

    c->_read_buffer_size = 4096;
    c->_read_buffer = malloc(c->_read_buffer_size * sizeof(char));
    assert(c->_read_buffer);

    return c;
}

// clear counts_table and free symbols_nodes, codes, intermediate_nodes
void huffman_clear(HuffmanCoder* coder) {
    memset(coder->counts_table, 0, sizeof(coder->counts_table));

    free(coder->symbols_nodes);
    coder->symbols_nodes = NULL;

    free(coder->codes);
    coder->codes = NULL;

    free(coder->intermediate_nodes);
    coder->intermediate_nodes = NULL;
}

void huffman_handle_file(HuffmanCoder* coder, FILE* file) {
    fseek(file, 0, SEEK_END);
    int64_t file_size = ftell(file);
    rewind(file);

    coder->read_progress = 0;

    uint64_t read_count = 0;
    uint64_t count;
    while ((count = fread(
        coder->_read_buffer,
        sizeof(char),
        coder->_read_buffer_size,
        file)
        )) {
        for (int i = 0; i < count; ++i) {
            coder->counts_table[coder->_read_buffer[i]]++;
        }
        read_count += count;
        coder->read_progress = (double)read_count / (double)file_size;
    }
    coder->read_progress = 1;
    rewind(file);
}

void huffman_build_codes(HuffmanCoder* coder) {
    l_generate_symbols_nodes(coder);
    Node* root = l_build_tree(coder);
    coder->root_node = root;
    coder->codes = l_get_codes(root, 256);
}


int read_bit(FILE* stream, unsigned char* buffer, int* bit_index) {
    if (*bit_index >= 8) {
        fread(buffer, sizeof(char), 1, stream);
        *bit_index = 0;
    }
    return (*buffer & (1 << (*bit_index)++)) != 0;
}

Node* l_load_code(
    FILE* stream,
    unsigned char* buffer,
    int* bit_index,
    Node* symbols_nodes,
    Node* inter_nodes,
    int* inter_nodes_i,
    int inter_nodes_count
) {
    if (read_bit(stream, buffer, bit_index)) {
        int symbol_value = 0;
        for (int i = 0; i < 8; ++i) {
            symbol_value |= (read_bit(stream, buffer, bit_index) << i);
        }
        assert(symbol_value >= 0 && symbol_value <= 255);
        return &symbols_nodes[symbol_value];
    }
    else {
        Node* cur = &inter_nodes[(*inter_nodes_i)++];
        cur->left =
            l_load_code(stream, buffer, bit_index, symbols_nodes, inter_nodes, inter_nodes_i, inter_nodes_count);
        cur->right =
            l_load_code(stream, buffer, bit_index, symbols_nodes, inter_nodes, inter_nodes_i, inter_nodes_count);
        cur->value = -1;

        return cur;
    }
}

void huffman_load_codes(HuffmanCoder* coder, FILE* stream) {

    l_generate_symbols_nodes(coder);
    Node* sub_nodes = calloc(256, sizeof(Node));
    coder->intermediate_nodes = sub_nodes;
    unsigned char buffer = 0;
    int bit_index = 8;
    int inter_nodes_i = 0;
    coder->root_node = l_load_code(
        stream,
        &buffer,
        &bit_index,
        coder->symbols_nodes,
        coder->intermediate_nodes,
        &inter_nodes_i,
        256
    );
    coder->codes = l_get_codes(coder->root_node, 256);
}


void append_bit(int bit_value, FILE* stream, unsigned char* buffer, int* bit_index) {
    if (*bit_index >= 8) {
        fwrite(buffer, sizeof(char), 1, stream);
        *buffer = 0;
        *bit_index = 0;
    }
    *buffer |= bit_value << (*bit_index)++;
}

void l_save_code(Node* node, FILE* stream, unsigned char* buffer, int* bit_index) {
    if (node->value != -1) {
        append_bit(1, stream, buffer, bit_index);
        for (int i = 0; i < 8; ++i) {
            append_bit((node->value & (1 << i)) != 0, stream, buffer, bit_index);
        }
    }
    else {
        append_bit(0, stream, buffer, bit_index);
        l_save_code(node->left, stream, buffer, bit_index);
        l_save_code(node->right, stream, buffer, bit_index);
    }
}

// return count of written bytes
uint64_t huffman_save_codes(HuffmanCoder* coder, FILE* stream) {
    uint64_t begin_pos = ftell(stream);

    unsigned char buffer = 0;
    int bit_index = 0;
    l_save_code(coder->root_node, stream, &buffer, &bit_index);
    if (bit_index > 0) { // fill to entire byte
        for (int i = bit_index - 1; i < 8; ++i) {
            append_bit(0, stream, &buffer, &bit_index);
        }
    }

    return ftell(stream) - begin_pos;
}

HuffmanCode huffman_encode_symbol(HuffmanCoder* coder, unsigned char symbol) {
    assert(coder->codes);
    return coder->codes[symbol];
}

void huffman_encode_symbols(
    HuffmanCoder* coder,
    FILE* stream,
    unsigned char* out_buffer,
    int out_buffer_len,
    int* out_bit_len,
    int* out_processed_bytes,
    int* stop_reason
) {
    int encoded_bits = 0;
    int buffer_remain = 0;
    int in_buffer_index = 0;
    int processed_bytes = 0;

    while (encoded_bits <= out_buffer_len * 8) {
        if (buffer_remain == 0) {
            in_buffer_index = 0;
            buffer_remain =
                (int)fread(
                    coder->_read_buffer,
                    sizeof(char),
                    coder->_read_buffer_size,
                    stream
                );
            processed_bytes += buffer_remain;
            if (buffer_remain == 0) {
                *stop_reason = 1;
                break;
            }
        }

        HuffmanCode code = huffman_encode_symbol(coder, coder->_read_buffer[in_buffer_index++]);

        bool is_stop = false;
        for (int i = 0; i < code.length; ++i) {
            out_buffer[encoded_bits / 8] |= (unsigned char)((code.code[i] == '1') << (encoded_bits % 8)); 
            encoded_bits++;
            if (encoded_bits == out_buffer_len * 8) {
                if (i < code.length - 1) { 
                    encoded_bits -= i + 1; 
                }
                else {
                    buffer_remain--;
                }
                is_stop = true;
                break;
            }
        }

        if (is_stop) {
            break;
        }

        buffer_remain--;
    }

    
    fseek(stream, -buffer_remain, SEEK_CUR);
    processed_bytes -= buffer_remain;

    if (*stop_reason == 0) {
        *stop_reason = 2;
    }

    *out_processed_bytes = processed_bytes;
    *out_bit_len = encoded_bits;
}
void huffman_decode_symbols(
    HuffmanCoder* coder,
    unsigned char* buffer,
    int buffer_bit_len,
    unsigned char* out_buffer,
    int* out_len
) {
    Node* current = coder->root_node;
    int bits = 0;
    int out_buffer_index = 0;
    for (int i = 0; i < buffer_bit_len / 8 + 1; ++i) {

        int to = int_min(8, buffer_bit_len - i * 8);
        for (int j = 0; j < to; ++j) { // loop for each bit in buffer
            bits++;
            if (buffer[i] & (1 << j)) {
                current = current->right;
            }
            else {
                current = current->left;
            }

            assert(current);
            if (current->value != -1) { // symbol founded
                out_buffer[out_buffer_index++] = (unsigned char)current->value;
                current = coder->root_node;
            }
        }
    }
    if (current != coder->root_node) {
        assert(0 && "The code is corrupted");
    }
    *out_len = out_buffer_index;
}



void huffman_free(HuffmanCoder* coder) {
    free(coder->_read_buffer);
    free(coder->codes);
    free(coder->intermediate_nodes);
    free(coder->symbols_nodes);
}

//////////////////////////////////////////////////////////////////////////

void file(char* input) {
    scanf_s("%s", input, sizeof(input));
}

void create(char* input, int* Verbose) {
    char FileName[50];
    char WhatKindOfFile[4];
    char NextCommand[10];
    scanf_s("%s", FileName, sizeof(FileName));
    scanf_s("%s", WhatKindOfFile, sizeof(WhatKindOfFile));
    if ((strcmp(WhatKindOfFile, "zip") != 0) || strcmp(WhatKindOfFile, "tar") != 0) {
        printf("Недопустимый формат файла! Требуется ввести формат \"zip\" или \"tar\"!\n");
        return;
    }
    scanf_s("%s", NextCommand, sizeof(NextCommand));
    char InCatalog[100];
    if ((strcmp(NextCommand, "--file") == 0) || strcmp(NextCommand, "-f") == 0) {
        file(InCatalog);
    }
    else {
        printf("Неверный аргумент команды, для просмотра команды используйте --help\n");
        return;
    }
    scanf_s("%s", NextCommand, sizeof(NextCommand));
    char OutCatalog[100];
    if ((strcmp(NextCommand, "--output") == 0) || strcmp(NextCommand, "-o") == 0) {
        file(OutCatalog);
    }
    else {
        printf("Неверный аргумент команды, для просмотра команды используйте --help\n");
        return;
    }
    scanf_s("%s", NextCommand, sizeof(NextCommand));
    int Size;
    if ((strcmp(NextCommand, "--size") == 0) || strcmp(NextCommand, "-s") == 0) {
        scanf_s("%d", &Size);
    }
    else {
        printf("Неверный аргумент команды, для просмотра команды используйте --help\n");
        return;
    }
    printf("%s\n", InCatalog);
    printf("%s\n", OutCatalog);
    printf("%d\n", Size);

    /*Сюда нужно добавить функции архивации и создания файла. входные данные для них:
    строка FileName - имя файла
    строка WhatKindOfFile - расширение (zip or tar)
    строка InCatalog - путь к архивируемому файлу
    строка OutCatalog - путь для сохранени архива
    инт Size - процент сжатия */

}

void extract(char* input, int* Verbose) {
    printf("команда 2 выполнена\n");
}

void list(char* input, int* Verbose) {
    printf("команда 3 выполнена\n");
}

void add(char* input, int* Verbose) {
    if (*Verbose == 1)
        printf("Вербоузик робит\n");
    printf("команда 4 выполнена\n");

}

void Delete(char* input, int* Verbose) {
    printf("команда 5 выполнена\n");
}

void help(char* input, int* Verbose) {
    printf("\n--create (-с)\t Команада для архивации файлов, используется по шаблону: [--create имя_создаваемого_файла тип_сжимаемого_файла --file имя_архивируемого_файла --output Путь_для_сохранения_сжатого_файла --size Требуемый_процент Сжатия)");
    printf("\n--extract (-e)\tКоманда для извлечения файлов из архива, используется по шаблону []");
    printf("\n--list (-l)\tКоманда для просмотра содержимого архива, используется по шаблону [--list путь_к_архиву]");

}

const char* commands[] = {
  "-c",
  "--create",
  "-x",
  "--extract",
  "-l",
  "--list",
  "-a",
  "--add",
  "-d",
  "--delete",
  "-h",
  "--help",
  "--exit"
};

void (*actions[10])(char*, int*) = {
  create,
  extract,
  list,
  add,
  Delete,
  help,
};

void executeCommand(char* command, int* exit, int* Verbose) {
    if (strcmp(command, "exit") == 0) { //Если команда выйти, то значение переменной выхода - 1
        *exit = 1;
        return;
    }
    if (strcmp(command, "--verbose") == 0 || strcmp(command, "-v") == 0) { //Если команда verbose, то значение переменной verbose - 1 если уже 1 то измен на 0
        if (*Verbose == 0) {
            *Verbose = 1;
            printf("Функция Verbose включена\n");
        }
        else
        {
            *Verbose = 0;
            printf("Функция Verbose выключена\n");
        }
        return;
    }
    for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) { //Любая другая команда проверяется
        if (strcmp(command, commands[i]) == 0) {
            actions[i / 2](command, Verbose);
            return;
        }
    }
    printf("Недопустимая команда, чтобы посмотреть список команд введите --help\n");
}

int main() {
    system("chcp 1251");
    char input[50];
    int exit = 0;
    int Verbose = 0;
    printf("Приветствуем Вас в нашем архиваторе! Чтобы получить информацию о работе в нем введите --help!\n");

    do {
        printf("Введите команду\n");
        scanf_s("%s", input, sizeof(input));
        executeCommand(input, &exit, &Verbose);
    } while (exit != 1);

    return 0;
}
