#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
_off64_t lseek64(int __filedes, _off64_t __offset, int __whence);

#define SECTOR_SIZE     2048
#define TOC_LBA         16
#define SYSTEM_CNF_NAME "SYSTEM.CNF;1"

struct dir_toc_entry
{
    short length;
    uint32_t fileLBA;         // 2
    uint32_t fileLBA_bigend;  // 6
    uint32_t fileSize;        // 10
    uint32_t fileSize_bigend; // 14
    uint8_t dateStamp[6];     // 18
    uint8_t reserved1;        // 24
    uint8_t fileProperties;   // 25
    uint8_t reserved2[6];     // 26
    uint8_t filenameLength;   // 32
    char filename[128];       // 33
} __attribute__((packed));

static unsigned char iso_buf[SECTOR_SIZE];

// Reads Primary Volume Descriptor from specified LBA and extracts root directory LBA
static int get_pvd(int fd, uint32_t *lba, int *length);

// Retrieves SYSTEM.CNF TOC entry using specified root directory TOC
static struct dir_toc_entry *get_toc_entry(int fd, uint32_t toc_lba, int toc_len);

// Reads SYSTEM.CNF from ISO image and copies it into given buffer
int read_system_cnf(const char *path, char *system_cnf_data, int bufSize)
{
    // Open ISO
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("ERROR: Failed to open file: %d\n", fd);
        return fd;
    }

    // Get location of root directory entry
    uint32_t root_lba = 0;
    int root_len = 0;
    if (get_pvd(fd, &root_lba, &root_len) != 0) {
        printf("ERROR: Failed to parse ISO PVD\n");
        close(fd);
        return -ENOENT;
    }

    // Get SYSTEM.CNF entry
    struct dir_toc_entry *toc_entry = get_toc_entry(fd, root_lba, root_len);
    if (toc_entry == NULL) {
        printf("ERROR: Failed to find SYSTEM.CNF\n");
        close(fd);
        return -EIO;
    }

    // Seek to SYSTEM.CNF location and read file contents
    int64_t res = lseek64(fd, (int64_t)toc_entry->fileLBA * SECTOR_SIZE, SEEK_SET);
    if (res < 0) {
        printf("ERROR: Failed to seek to SYSTEM.CNF\n");
        close(fd);
        return errno;
    }

    res = read(fd, system_cnf_data, bufSize);
    if ((res < toc_entry->length) || (res < bufSize)) {
        res = -EIO;
    } else
        res = 0;

    close(fd);
    return 0;
}

// Reads Primary Volume Descriptor from specified LBA and extracts root directory LBA
static int get_pvd(int fd, uint32_t *lba, int *length)
{
    // Seek to PVD LBA
    int64_t res = lseek64(fd, (int64_t)TOC_LBA * SECTOR_SIZE, SEEK_SET);
    if (res < 0) {
        return -EIO;
    }
    // Read the sector
    if (read(fd, iso_buf, SECTOR_SIZE) == SECTOR_SIZE) {
        // Make sure the sector contains PVD (type code 1, identifier CD001)
        if ((iso_buf[0x00] == 1) && (!memcmp(&iso_buf[0x01], "CD001", 5))) {
            // Read root directory entry and get LBA and length
            struct dir_toc_entry *toc_entry_ptr = (struct dir_toc_entry *)&iso_buf[0x9c];
            *lba = toc_entry_ptr->fileLBA;
            *length = toc_entry_ptr->length;
            return 0;
        } else {
            return -EINVAL;
        }
    }
    return -EIO;
}

// Retrieves SYSTEM.CNF TOC entry using specified root directory TOC
static struct dir_toc_entry *get_toc_entry(int fd, uint32_t toc_lba, int toc_len)
{
    // Read TOC entries
    int64_t res = 0;
    while (toc_len > 0) {
        // Seek to next LBA
        res = lseek64(fd, (int64_t)toc_lba * SECTOR_SIZE, SEEK_SET);
        if (res < 0) {
            return NULL;
        }
        // Read the sector
        if (read(fd, iso_buf, SECTOR_SIZE) != SECTOR_SIZE) {
            return NULL;
        }

        // Read directory entries until the end of sector
        int tocPos = 0;
        struct dir_toc_entry *toc_entry_ptr;
        do {
            toc_entry_ptr = (struct dir_toc_entry *)&iso_buf[tocPos];

            if (toc_entry_ptr->length == 0)
                break;

            if (toc_entry_ptr->filenameLength && !strcmp(SYSTEM_CNF_NAME, toc_entry_ptr->filename)) {
                // File has been found
                return toc_entry_ptr;
            }
            // Advance to the next entry
            tocPos += (toc_entry_ptr->length << 16) >> 16;
        } while (tocPos < 2016);

        // Get next sector LBA
        toc_len -= SECTOR_SIZE;
        toc_lba++;
    }

    return NULL;
}
