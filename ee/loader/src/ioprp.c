#include "ioprp.h"
#include "modlist.h"
#include <stdio.h>
#include <string.h>


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

/*----------------------------------------------------------------------------------------
    IOPRP image data
    Two variants:
      ioprp_img_full - includes CDVDMAN + CDVDFSV (used when DVD emulation is active)
      ioprp_img_dvd  - EESYNC only              (used when DVD emulation is not active)
------------------------------------------------------------------------------------------*/
const struct ioprp_img_full ioprp_img_full = {
    {{"RESET"  ,  8, 0},
     {"ROMDIR" ,  0, 0x10 * 7},
     {"EXTINFO",  0, sizeof(struct ioprp_ext_full)},
     {"CDVDMAN", 28, 0},
     {"CDVDFSV", 32, 0},
     {"EESYNC" , 24, 0},
     {"", 0, 0}},
    {
        // RESET extinfo
        {0, 4, EXTINFO_TYPE_DATE},
        0x20230621,
        // CDVDMAN extinfo
        {0, 4, EXTINFO_TYPE_DATE},
        0x20230621,
        {0x9999, 0, EXTINFO_TYPE_VERSION},
        {0, 12, EXTINFO_TYPE_COMMENT},
        "cdvd_driver",
        // CDVDFSV extinfo
        {0, 4, EXTINFO_TYPE_DATE},
        0x20230621,
        {0x9999, 0, EXTINFO_TYPE_VERSION},
        {0, 16, EXTINFO_TYPE_COMMENT},
        "cdvd_ee_driver",
        // SYNCEE extinfo
        {0, 4, EXTINFO_TYPE_DATE},
        0x20230621,
        {0x9999, 0, EXTINFO_TYPE_VERSION},
        {0, 8, EXTINFO_TYPE_COMMENT},
        "SyncEE"
    }};

const struct ioprp_img_dvd ioprp_img_dvd = {
    {{"RESET"  ,  8, 0},
     {"ROMDIR" ,  0, 0x10 * 5},
     {"EXTINFO",  0, sizeof(struct ioprp_ext_dvd)},
     {"EESYNC" , 24, 0},
     {"", 0, 0}},
    {
        // RESET extinfo
        {0, 4, EXTINFO_TYPE_DATE},
        0x20230621,
        // SYNCEE extinfo
        {0, 4, EXTINFO_TYPE_DATE},
        0x20230621,
        {0x9999, 0, EXTINFO_TYPE_VERSION},
        {0, 8, EXTINFO_TYPE_COMMENT},
        "SyncEE"
    }};

/*----------------------------------------------------------------------------------------
    Replace modules in IOPRP image:
    - CDVDMAN
    - CDVDFSV
    - EESYNC
------------------------------------------------------------------------------------------*/
unsigned int patch_IOPRP_image(struct romdir_entry *romdir_out,
                               const struct romdir_entry *romdir_in,
                               struct SModList *ml)
{
    struct romdir_entry *romdir_out_org = romdir_out;
    uint8_t *ioprp_in  = (uint8_t *)romdir_in;
    uint8_t *ioprp_out = (uint8_t *)romdir_out;

    while (romdir_in->name[0] != '\0') {
        struct SModule *mod = modlist_get_by_udnlname(ml, romdir_in->name);
        if (mod != NULL) {
            printf("IOPRP: replacing %s with %s\n", romdir_in->name, mod->sFileName);
            memcpy(ioprp_out, mod->pData, mod->iSize);
            romdir_out->size = mod->iSize;
        } else {
            printf("IOPRP: keeping %s\n", romdir_in->name);
            memcpy(ioprp_out, ioprp_in, romdir_in->size);
            romdir_out->size = romdir_in->size;
        }

        // Align all addresses to a multiple of 16
        ioprp_in  += (romdir_in->size  + 0xF) & ~0xF;
        ioprp_out += (romdir_out->size + 0xF) & ~0xF;
        romdir_in++;
        romdir_out++;
    }

    return (ioprp_out - (uint8_t *)romdir_out_org);
}
