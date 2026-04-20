#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Minimal ELF32 definitions so this works in a freestanding kernel
 * without relying on <elf.h>.
 */

#define EI_NIDENT     16

/* e_ident[] indexes */
#define EI_MAG0       0
#define EI_MAG1       1
#define EI_MAG2       2
#define EI_MAG3       3
#define EI_CLASS      4
#define EI_DATA       5
#define EI_VERSION    6

/* magic */
#define ELFMAG0       0x7f
#define ELFMAG1       'E'
#define ELFMAG2       'L'
#define ELFMAG3       'F'

/* ELF class */
#define ELFCLASS32    1

/* endianness */
#define ELFDATA2LSB   1

/* ELF version */
#define EV_CURRENT    1

/* object type */
#define ET_REL        1

/* machine */
#define EM_ARM        40

/* section types */
#define SHT_NULL      0
#define SHT_PROGBITS  1
#define SHT_SYMTAB    2
#define SHT_STRTAB    3
#define SHT_RELA      4
#define SHT_NOBITS    8
#define SHT_REL       9

/* section flags */
#define SHF_WRITE     0x1
#define SHF_ALLOC     0x2
#define SHF_EXECINSTR 0x4

#pragma pack(1)
typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint32_t      e_entry;
    uint32_t      e_phoff;
    uint32_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} Elf32_Shdr;
#pragma pop

/* Returns 1 if [off, off+len) is within [0, total). */
static int elf_range_ok(size_t off, size_t len, size_t total) {
    return (off <= total) && (len <= (total - off));
}

/* Generic overflow-safe align-up helper. */
static int elf_align_up(size_t value, size_t align, size_t *out) {
    size_t rem;
    size_t add;

    if (out == NULL) {
        return 0;
    }

    if (align == 0 || align == 1) {
        *out = value;
        return 1;
    }

    rem = value % align;
    if (rem == 0) {
        *out = value;
        return 1;
    }

    add = align - rem;
    if (value > (SIZE_MAX - add)) {
        return 0;
    }

    *out = value + add;
    return 1;
}

/*
 * Load all SHF_ALLOC sections from an ELF32 ET_REL object into a packed
 * in-memory image starting at base_addr.
 *
 * Returns:
 *   >0  = total bytes consumed in destination image
 *    0  = error
 *
 * Notes:
 * - Sections are laid out in section-header order.
 * - SHT_NOBITS sections are zero-filled.
 * - Non-ALLOC sections (e.g. .symtab/.strtab/.rel.*) are ignored.
 * - No relocations are applied here.
 */
size_t elf32_load_alloc_sections(const void *elf_file,
                                 size_t elf_size,
                                 void *base_addr)
{
    Elf32_Ehdr eh;
    const uint8_t *src;
    uint8_t *dst;
    size_t sh_table_size;
    size_t cursor;
    uint16_t i;

    if (elf_file == NULL) {
        return 0;
    }

    if (base_addr == NULL) {
        printf("Output is NULL, printing all writes\n");
    }

    if (elf_size < sizeof(Elf32_Ehdr)) {
        printf("bad header size\n");
        return 0;
    }

    src = (const uint8_t *)elf_file;
    dst = (uint8_t *)base_addr;

    memcpy(&eh, src, sizeof(eh));

    /* Basic ELF validation */
    if (eh.e_ident[EI_MAG0] != ELFMAG0 ||
        eh.e_ident[EI_MAG1] != ELFMAG1 ||
        eh.e_ident[EI_MAG2] != ELFMAG2 ||
        eh.e_ident[EI_MAG3] != ELFMAG3) {
        printf("bad ID %02x %02x %02x %02x \n", eh.e_ident[EI_MAG0], eh.e_ident[EI_MAG1], eh.e_ident[EI_MAG2], eh.e_ident[EI_MAG3]);
        return 0;
    }

    if (eh.e_ident[EI_CLASS] != ELFCLASS32) {
        printf("bad ID\n");
        return 0;
    }

    if (eh.e_ident[EI_DATA] != ELFDATA2LSB) {
        printf("bad data format\n");
        return 0;
    }

    if (eh.e_ident[EI_VERSION] != EV_CURRENT || eh.e_version != EV_CURRENT) {
        printf("bad version\n");
        return 0;
    }

    if (eh.e_type != ET_REL) {
        printf("bad type\n");
        return 0;
    }

    if (eh.e_machine != EM_ARM) {
        printf("bad platform\n");
        return 0;
    }

    if (eh.e_ehsize != sizeof(Elf32_Ehdr)) {
        printf("bad header size\n");
        return 0;
    }

    if (eh.e_shentsize != sizeof(Elf32_Shdr)) {
        printf("bad section size\n");
        return 0;
    }

    sh_table_size = (size_t)eh.e_shnum * (size_t)eh.e_shentsize;
    if (!elf_range_ok((size_t)eh.e_shoff, sh_table_size, elf_size)) {
        printf("bad range\n");
        return 0;
    }

    cursor = 0;
    printf("%i\n", eh.e_shnum);
    for (i = 0; i < eh.e_shnum; ++i) {
        Elf32_Shdr sh;
        size_t sh_off;
        size_t sec_dst_off;
        size_t sec_size;
        size_t sec_align;

        sh_off = (size_t)eh.e_shoff + ((size_t)i * sizeof(Elf32_Shdr));
        memcpy(&sh, src + sh_off, sizeof(sh));

        /* Skip non-alloc sections */
        if ((sh.sh_flags & SHF_ALLOC) == 0) {
            continue;
        }

        sec_size  = (size_t)sh.sh_size;
        sec_align = (sh.sh_addralign == 0) ? 1u : (size_t)sh.sh_addralign;

        if (!elf_align_up(cursor, sec_align, &sec_dst_off)) {
            return 0;
        }

        /* Bounds-check source data for sections that occupy file space */
        if (sh.sh_type != SHT_NOBITS) {
            if (!elf_range_ok((size_t)sh.sh_offset, sec_size, elf_size)) {
                return 0;
            }
        }

        /* Bounds-check destination arithmetic */
        if (sec_dst_off > SIZE_MAX - sec_size) {
            return 0;
        }

        if (sh.sh_type == SHT_NOBITS) {
            /* .bss-style section: not present in file, zero in memory */
            if ( base_addr != NULL ) {
                memset(dst + sec_dst_off, 0, sec_size);
            }  else {
                for ( int i = 0; i < sec_size/4; i++ ) {
                    printf("[%llu] = 0x00000000\n");
                }
            }
        } else {
            if ( base_addr != NULL ) {
                memcpy(dst + sec_dst_off, src + sh.sh_offset, sec_size);
            }  else {
                for ( int i = 0; i < sec_size/4; i++ ) {
                    printf("[%llu] = 0x%08x\n", src[sh.sh_offset + i*4]);
                }
                if ( (sec_size - (sec_size&(~0x3))) > 0 ) {
                    printf("[%llu] = 0x%08x\n", src[sh.sh_offset + (sec_size - (sec_size&(0x3)))]);
                }
            }
        }

        printf("aaa\n");

        cursor = sec_dst_off + sec_size;
    }

    return cursor;
}