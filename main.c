#include <stdbool.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// Each entry in this lookup table indicates whether a character is a valid
// identifier character. The table is indexed by the ASCII value of the
// character.
static const uint8_t checks[256] = {
//  0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 0x
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 1x
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 2x
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   0,   0,   0,   0,   0,   0, // 3x
    0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // 4x
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   0,   0,   0,   0, 255, // 5x
    0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // 6x
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   0,   0,   0,   0,   0, // 7x
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 8x
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 9x
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // Ax
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // Bx
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // Cx
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // Dx
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // Ex
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // Fx
};

#if defined(NEON_RANGES)
#include <arm_neon.h>

/**
 * Returns a pointer to the first byte following the end of the identifier that
 * started just before the given source pointer.
 */
static const unsigned char * parse_identifier(const unsigned char *source, const unsigned char *end) {
    while (source < end) {
        if ((end - source) >= 16) {
            uint8x16_t chars = vld1q_u8(source);
            uint64x2_t checks =
                vreinterpretq_u64_u8(
                    vorrq_u8(
                        vorrq_u8(
                            vorrq_u8(
                                vandq_u8(vcgeq_u8(chars, vdupq_n_u8('a')), vcleq_u8(chars, vdupq_n_u8('z'))),
                                vandq_u8(vcgeq_u8(chars, vdupq_n_u8('A')), vcleq_u8(chars, vdupq_n_u8('Z')))
                            ),
                            vandq_u8(vcgeq_u8(chars, vdupq_n_u8('0')), vcleq_u8(chars, vdupq_n_u8('9')))
                        ),
                        vceqq_u8(chars, vdupq_n_u8('_'))
                    )
                );

            uint64_t u64_0 = vgetq_lane_u64(checks, 0);
            if (u64_0 != UINT64_MAX) return source + (__builtin_ctzll(~u64_0) / 8);

            uint64_t u64_1 = vgetq_lane_u64(checks, 1);
            if (u64_1 != UINT64_MAX) return source + 8 + (__builtin_ctzll(~u64_1) / 8);

            source += 16;
        } else {
            while (source < end && checks[*source]) source++;
            return source;
        }
    }

    // Reached the end of the string.
    return end;
}
#elif defined(NEON_LOOKUP)
#include <arm_neon.h>
static uint8x16x4_t table[2];

static void prepare(void) {
    table[0].val[0] = vld1q_u8(checks);
    table[0].val[1] = vld1q_u8(checks + 0x10);
    table[0].val[2] = vld1q_u8(checks + 0x20);
    table[0].val[3] = vld1q_u8(checks + 0x30);
    table[1].val[0] = vld1q_u8(checks + 0x40);
    table[1].val[1] = vld1q_u8(checks + 0x50);
    table[1].val[2] = vld1q_u8(checks + 0x60);
    table[1].val[3] = vld1q_u8(checks + 0x70);
}

/**
 * Returns a pointer to the first byte following the end of the identifier that
 * started just before the given source pointer.
 */
static const unsigned char * parse_identifier(const unsigned char *source, const unsigned char *end) {
    while (source < end) {
        if ((end - source) >= 16) {
            uint8x16_t chars = vld1q_u8(source);
            uint64x2_t result =
                vreinterpretq_u64_u8(
                    vorrq_u8(
                        vqtbl4q_u8(table[0], chars),
                        vqtbl4q_u8(table[1], veorq_u8(chars, vdupq_n_u8(0x40)))
                    )
                );

            uint64_t u64_0 = vgetq_lane_u64(result, 0);
            if (u64_0 != UINT64_MAX) return source + (__builtin_ctzll(~u64_0) / 8);

            uint64_t u64_1 = vgetq_lane_u64(result, 1);
            if (u64_1 != UINT64_MAX) return source + 8 + (__builtin_ctzll(~u64_1) / 8);

            source += 16;
        } else {
            while (source < end && checks[*source]) source++;
            return source;
        }
    }

    // Reached the end of the string.
    return end;
}
#else
/**
 * Returns a pointer to the first byte following the end of the identifier that
 * started just before the given source pointer.
 */
static const unsigned char * parse_identifier(const unsigned char *source, const unsigned char *end) {
    while (source < end && checks[*source]) source++;
    return source;
}
#endif

/**
 * Count the number of identifiers in the given source string and print out the
 * count to stdout.
 */
static ssize_t parse_identifiers(const unsigned char *source, size_t size) {
    ssize_t count = 0;
    const unsigned char *end = source + size;

    while (source < end) {
        switch (*source++) {
            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
            case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
            case 's': case 't': case 'u': case 'v': case 'w': case 'x':
            case 'y': case 'z':
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
            case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
            case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
            case 'Y': case 'Z':
            case '_': {
                count++;
                source = parse_identifier(source, end);
                break;
            }
        }
    }

    return count;
}

/**
 * Load the contents of the file specificed by the given filepath into the out
 * parameters. The caller is responsible for freeing the memory allocated for
 * the file contents.
 */
static bool load_file(const char *filepath, const unsigned char **source, size_t *size) {
    // Open the file for reading
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return false;
    }

    // Stat the file to get the file size
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        perror("fstat");
        return false;
    }

    // mmap the file descriptor to virtually get the contents
    *size = (size_t) sb.st_size;
    if (*size == 0) {
        *source = (const unsigned char *) "";
        close(fd);
        return true;
    }

    *source = mmap(NULL, *size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (*source == MAP_FAILED) {
        perror("Map failed");
        return false;
    }

    close(fd);
    return true;
}

/**
 * Unload the file contents from memory.
 */
static void unload_file(const unsigned char **source, size_t *size) {
    void *memory = (void *) *source;
    munmap(memory, *size);
}

#define PARSE_FILE_FAILURE -1

/**
 * Parses the given file and prints the number of identifiers in the file to
 * stdout.
 */
static ssize_t parse_file(const char *filepath) {
    const unsigned char *source = NULL;
    size_t size;

    if (load_file(filepath, &source, &size)) {
        ssize_t count = parse_identifiers(source, size);
        unload_file(&source, &size);
        return count;
    }

    return PARSE_FILE_FAILURE;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filepath>\n", argv[0]);
        return EXIT_FAILURE;
    }

#ifdef NEON_LOOKUP
    prepare();
#endif

    ssize_t count = parse_file(argv[1]);
    if (count == PARSE_FILE_FAILURE) {
        return EXIT_FAILURE;
    }

    printf("%zd\n", count);
    return EXIT_SUCCESS;
}
