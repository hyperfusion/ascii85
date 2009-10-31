#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define FAIL(message, format) { \
    fprintf(stderr, message, format); \
    exit(1); \
}

unsigned short arg_decode = 0;
unsigned short arg_use_markers = 1;

const unsigned long p85[] = { 85*85*85*85, 85*85*85, 85*85, 85, 1 };

void encode_block(unsigned long block, unsigned short fill, FILE *out) {
    int i;
    char block85[5];
    char *curr_digit = block85;

    /* Transform the block into a base 85 number,
       putting each base-85 digit into the block85 array */
    for (i = 5; i > 0; --i) {
        *curr_digit = block % 85; /* eval one b85 digit */
        block /= 85; /* prepare for next b85 digit */
        ++curr_digit; /* go to next digit pointer */
    }
    /* Output the base-85 #'s, adding 33 ('!') to make
       them ASCII (we're going backwards because
       the transformation algorithm transforms digits
       in reverse) */
    for (i = fill; i >= 0; --i) {
        --curr_digit; /* go to prev digit (see above for reason) */
        fputc(*curr_digit + '!', out); /* print the digit, adding 33 to make it ascii */
    }
}

void encode(FILE *data, FILE *out) {
    unsigned long block;
    int curr_char;
    unsigned short fill;

    block = fill = 0;

    if (arg_use_markers)
        fprintf(out, "<~");

    /* Start encoding */
    while ((curr_char = fgetc(data)) != EOF) {
        /* Here, we fill up the block/tuple */
        switch (fill++) {
        case 0: block |= (curr_char << 24); break;
        case 1: block |= (curr_char << 16); break;
        case 2: block |= (curr_char << 8);  break;
        case 3:
            /* Once the block is filled, encode it */
            block |= curr_char;
            /* If the block is 0, output a "z" instead of "!!!!!".
               Otherwise, encode it normally. */
            if (block == 0)
                fputc('z', out);
            else
                encode_block(block, fill, out);

            /* Reset everything for the next block */
            block = fill = 0;
            break;
        }
    }
    /* Encode any leftover bits */
    if (fill > 0)
        encode_block(block, fill, out);

    if (arg_use_markers)
        fprintf(out, "~>");

    fputc('\n', out);
}

void decode_block(unsigned long block, unsigned short fill, FILE *out) {
    switch (fill) {
    case 4:
        fputc(block >> 24, out);
        fputc(block >> 16, out);
        fputc(block >>  8, out);
        fputc(block      , out);
        break;
    case 3:
        fputc(block >> 24, out);
        fputc(block >> 16, out);
        fputc(block >>  8, out);
        break;
    case 2:
        fputc(block >> 24, out);
        fputc(block >> 16, out);
        break;
    case 1:
        fputc(block >> 24, out);
        break;
    }
}

void decode_last_block(unsigned long block, unsigned short fill, FILE *out) {
    if (fill > 0) {
        --fill;
        block += p85[fill];
        decode_block(block, fill, out);
    }
}

void decode(FILE *data, FILE *out) {
    unsigned long block;
    int curr_char;
    unsigned short fill;

    if (arg_use_markers)
        if ((curr_char = fgetc(data)) != EOF && curr_char == '<')
            if ((curr_char = fgetc(data)) != '~')
                if (curr_char == EOF)
                    return; /* no data in, so no data out */
                else {
                    fprintf(stderr, "invalid ascii85 starting block\n");
                    exit(1);
                }

    block = fill = 0;

    for (;;) {
        switch (curr_char = fgetc(data)) {
        default:
            if (isspace(curr_char)) break;

            if (curr_char < '!' || curr_char > 'u') {
                fprintf(stderr, "bad char in region %#o\n", curr_char);
                exit(1);
            }
            block += (curr_char - '!') * p85[fill];
            ++fill;
            if (fill == 5) {
                decode_block(block, 4, out);
                block = fill = 0;
            }
            break;

        case 'z':
            if (fill != 0) {
                fprintf(stderr, "\"z\" found within ascii85 block at position %d\n", ftell(data));
                exit(1);
            }
            fputc(0, out);
            fputc(0, out);
            fputc(0, out);
            fputc(0, out);
            break;

        case '~': /* end of data marker */
            /* if the end-of-data marker is required, interpret
               this as the end of the data */
            if (arg_use_markers) {
                /* check if marker is valid */
                if (fgetc(data) == '>') {
                    /* decode last block, if incomplete */
                    decode_last_block(block, fill, out);
                    return;
                }
                fprintf(stderr, "incomplete ending marker at position %d\n", ftell(data));
                exit(1);
            }
            break;

        case EOF:
            /* if no file markers are required, take this as ~> */
            if (!arg_use_markers) {
                decode_last_block(block, fill, out);
                return;
            }
            fprintf(stderr, "EOF found inside ascii85 block at position %d\n", ftell(data));
            exit(1);
        }
    }
}

void help(const char *progname) {
    FAIL("Usage: %s [OPTION]... input output\n"
         "Performs ASCII85 encoding and decoding.\n"
         "  -d      decode the given data (default is encode)\n"
         "  -m      don't use the <~ and ~> markers\n"
         "  -?      print this help message\n", progname);
}

void usage(const char *progname) {
    FAIL("Usage: %s [-d] [-m] [-?] input output\n", progname);
}

int main(int argc, char *argv[]) {
    int curr_arg;
    FILE *fin, *fout;

    while ((curr_arg = getopt(argc, argv, "dm?")) != EOF) {
        switch (curr_arg) {
        case 'd':
            arg_decode = 1;
            break;
        case 'm':
            arg_use_markers = 0;
            break;
        case '?':
            help(argv[0]);
            break;
        }
    }

    /* If no input/output files are specified, use std{in,out} */
    if (optind == argc) {
        fin = stdin;
        fout = stdout;
        goto action;
    }

    if (strcmp(argv[optind], "-") == 0)
        fin = stdin;
    else if ((fin = fopen(argv[optind], "r")) == NULL)
        FAIL("couldn't open file \"%s\" for reading\n", argv[optind]);

    /* If the output file isn't specified, use stdout */
    ++optind; /* lazy hack */
    if (argc == optind  || strcmp(argv[optind], "-") == 0)
        fout = stdout;
    else if ((fout = fopen(argv[optind], "w")) == NULL)
        FAIL("couldn't open file \"%s\" for writing\n", argv[optind]);

action:
    if (arg_decode)
        decode(fin, fout);
    else
        encode(fin, fout);

    fclose(fin);
    fclose(fout);

    return 0;
}
