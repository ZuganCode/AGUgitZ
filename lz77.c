#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

unsigned long lz77_compress (unsigned char *uncompressed_text, unsigned long uncompressed_size, unsigned char *compressed_text, unsigned char pointer_length_width)
{
    unsigned int pointer_pos, temp_pointer_pos, output_pointer, pointer_length, temp_pointer_length;
    unsigned long compressed_pointer, output_size, coding_pos, output_lookahead_ref, look_behind, look_ahead;
    unsigned int pointer_pos_max, pointer_length_max;
    pointer_pos_max = pow(2, 16 - pointer_length_width);
    pointer_length_max = pow(2, pointer_length_width);

    *((unsigned long *) compressed_text) = uncompressed_size;
    *(compressed_text + 4) = pointer_length_width;
    compressed_pointer = output_size = 5;

    for(coding_pos = 0; coding_pos < uncompressed_size; ++coding_pos)
    {
        pointer_pos = 0;
        pointer_length = 0;
        for(temp_pointer_pos = 1; (temp_pointer_pos < pointer_pos_max) && (temp_pointer_pos <= coding_pos); ++temp_pointer_pos)
        {
            look_behind = coding_pos - temp_pointer_pos;
            look_ahead = coding_pos;
            for(temp_pointer_length = 0; uncompressed_text[look_ahead++] == uncompressed_text[look_behind++]; ++temp_pointer_length)
                if(temp_pointer_length == pointer_length_max)
                    break;
            if(temp_pointer_length > pointer_length)
            {
                pointer_pos = temp_pointer_pos;
                pointer_length = temp_pointer_length;
                if(pointer_length == pointer_length_max)
                    break;
            }
        }
        coding_pos += pointer_length;
        if((coding_pos == uncompressed_size) && pointer_length)
        {
            output_pointer = (pointer_length == 1) ? 0 : ((pointer_pos << pointer_length_width) | (pointer_length - 2));
            output_lookahead_ref = coding_pos - 1;
        }
        else
        {
            output_pointer = (pointer_pos << pointer_length_width) | (pointer_length ? (pointer_length - 1) : 0);
            output_lookahead_ref = coding_pos;
        }
        *((unsigned int *) (compressed_text + compressed_pointer)) = output_pointer;
        compressed_pointer += 2;
        *(compressed_text + compressed_pointer++) = *(uncompressed_text + output_lookahead_ref);
        output_size += 3;
    }

    return output_size;
}

unsigned long lz77_decompress (unsigned char *compressed_text, unsigned char *uncompressed_text)
{
    unsigned char pointer_length_width;
    unsigned int input_pointer, pointer_length, pointer_pos, pointer_length_mask;
    unsigned long compressed_pointer, coding_pos, pointer_offset, uncompressed_size;

    uncompressed_size = *((unsigned long *) compressed_text);
    pointer_length_width = *(compressed_text + 4);
    compressed_pointer = 5;

    pointer_length_mask = pow(2, pointer_length_width) - 1;

    for(coding_pos = 0; coding_pos < uncompressed_size; ++coding_pos)
    {
        input_pointer = *((unsigned int *) (compressed_text + compressed_pointer));
        compressed_pointer += 2;
        pointer_pos = input_pointer >> pointer_length_width;
        pointer_length = pointer_pos ? ((input_pointer & pointer_length_mask) + 1) : 0;
        if(pointer_pos)
            for(pointer_offset = coding_pos - pointer_pos; pointer_length > 0; --pointer_length)
                uncompressed_text[coding_pos++] = uncompressed_text[pointer_offset++];
        *(uncompressed_text + coding_pos) = *(compressed_text + compressed_pointer++);
    }

    return coding_pos;
}

long fsize (FILE *in)
{
    long pos, length;
    pos = ftell(in);
    fseek(in, 0L, SEEK_END);
    length = ftell(in);
    fseek(in, pos, SEEK_SET);
    return length;
}

unsigned long file_lz77_decompress (char *filename_in, char *filename_out)
{
    FILE *in, *out;
    unsigned long compressed_size, uncompressed_size;
    unsigned char *compressed_text, *uncompressed_text;

    in = fopen(filename_in, "r");
    if(in == NULL)
        return 0;
    compressed_size = fsize(in);
    compressed_text = malloc(compressed_size);
    if(fread(compressed_text, 1, compressed_size, in) != compressed_size)
        return 0;
    fclose(in);

    uncompressed_size = *((unsigned long *) compressed_text);
    uncompressed_text = malloc(uncompressed_size);

    if(lz77_decompress(compressed_text, uncompressed_text) != uncompressed_size)
        return 0;

    out = fopen(filename_out, "w");
    if(out == NULL)
        return 0;
    if(fwrite(uncompressed_text, 1, uncompressed_size, out) != uncompressed_size)
        return 0;
    fclose(out);

    return uncompressed_size;
}

// Structureore input data
typedef struct {
    char *inputFile;
    char *outputFile;
    float compressionPercentage;
} InputData;

// Function to get user input
InputData getUserInput() {
    InputData data;

    printf("Введите имя файла для сжатия: ");
    data.inputFile = malloc(255 * sizeof(char));
    scanf("%s", data.inputFile);

    printf("Введите имя сжатого файла: ");
    data.outputFile = malloc(255 * sizeof(char));
    scanf("%s", data.outputFile);

    printf("Введите желаемый процент сжатия файла (0-100%%): ");
    scanf("%f", &data.compressionPercentage);

    return data;
}

// Function to compress a file
unsigned long file_lz77_compress(char *filename_in, char *filename_out, size_t malloc_size, unsigned char pointer_length_width) {
    FILE *in, *out;
    unsigned char *uncompressed_text, *compressed_text;
    unsigned long uncompressed_size, compressed_size;

    // Open input file
    in = fopen(filename_in, "r");
    if (in == NULL) {
        return 0;
    }

    // Get uncompressed file size
    uncompressed_size = fsize(in);

    // Allocate memory for uncompressed text
    uncompressed_text = malloc(uncompressed_size);
    if ((uncompressed_size != fread(uncompressed_text, 1, uncompressed_size, in))) {
        free(uncompressed_text);
        fclose(in);
        return 0;
    }

    // Close input file
    fclose(in);

    // Allocate memory for compressed text
    compressed_text = malloc(malloc_size);

    // Perform LZ77 compression
    compressed_size = lz77_compress(uncompressed_text, uncompressed_size, compressed_text, pointer_length_width);

    // Open output file
    out = fopen(filename_out, "w");
    if (out == NULL) {
        free(uncompressed_text);
        free(compressed_text);
        return 0;
    }

    // Write compressed data to output file
    if ((compressed_size != fwrite(compressed_text, 1, compressed_size, out))) {
        free(uncompressed_text);
        free(compressed_text);
        fclose(out);
        return 0;
    }

    // Close output file
    fclose(out);

    // Free memory
    free(uncompressed_text);
    free(compressed_text);

    return compressed_size;
}

int main() 
{
    InputData inputData;
    long originalSize;
    unsigned long compressedSize;
    float actualCompressionPercentage;

    // Get user input
    inputData = getUserInput();

    // Get original file size
    FILE *in = fopen(inputData.inputFile, "rb");
    if (in == NULL) {
        perror("Error opening file.");
        return 1;
    }
    originalSize = fsize(in);
    fclose(in);

    // Compress the file
    compressedSize = file_lz77_compress(inputData.inputFile, inputData.outputFile, 10000000, 8);

    if (compressedSize == 0) {
        printf("Error compressing file.\n");
        return 1;
    }

    // Calculate actual compression percentage
    actualCompressionPercentage = 100.0 - ((float)compressedSize / originalSize * 100.0);

    if (actualCompressionPercentage >= inputData.compressionPercentage) {
        printf("Compression successful. Desired compression percentage achieved: %f%%\n", actualCompressionPercentage);
    } else {
        printf("Desired compression percentage not achieved. Current compression percentage: %f%%\n", actualCompressionPercentage);
    }

    return 0;
}