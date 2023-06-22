#include "ioprp.h"
#include <stdio.h>


void print_extinfo(const uint8_t *data, uint32_t size)
{
    struct extinfo * ext;

    while (size > 0) {
        ext = (struct extinfo *)data;

        if (size < sizeof(struct extinfo)) {
            printf("- -> ERROR: extsize1 %ld < 4\n", size);
            return;
        }

        if (size < (sizeof(struct extinfo) + ext->ext_length)) {
            printf("- -> ERROR: extsize2 %ld < (4 + %d)\n", size, ext->ext_length);
            return;
        }

        switch(ext->type) {
            case EXTINFO_TYPE_DATE:
                if (ext->ext_length == 0)
                    printf("- -> DATE = 0x%x\n", ext->value);
                else if (ext->ext_length == 4)
                    printf("- -> DATE = 0x%lx\n", *(uint32_t *)&data[sizeof(struct extinfo)]);
                else
                    printf("- -> DATE ???\n");
                break;
            case EXTINFO_TYPE_VERSION:
                printf("- -> VERSION = 0x%x\n", ext->value);
                break;
            case EXTINFO_TYPE_COMMENT:
                printf("- -> COMMENT = %s\n", &data[sizeof(struct extinfo)]);
                break;
            case EXTINFO_TYPE_NULL:
                printf("- -> NULL\n");
                break;
            default:
                printf("- -> ???\n");
        };

        size -= sizeof(struct extinfo) + ext->ext_length;
        data += sizeof(struct extinfo) + ext->ext_length;
    }
}

void print_romdir(const struct romdir_entry *romdir)
{
    uint8_t *data = (uint8_t *)romdir;
    data += romdir[1].size; // FIXME: The offset where the extdata starts

    while (romdir->name[0] != '\0') {
        printf("- %s [size=%ld, extsize=%d]\n", romdir->name, romdir->size, romdir->extinfo_size);

        if (romdir->extinfo_size > 0) {
            print_extinfo(data, romdir->extinfo_size);
            data += romdir->extinfo_size;
        }

        romdir++;
    }
}
