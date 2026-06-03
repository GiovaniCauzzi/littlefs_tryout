#include "./external/littlefs/lfs.h"

enum
{
    QUIET,
    VERBOSE,
};


// variables used by the filesystem
lfs_t lfs;
lfs_file_t file;


#define BLOCK_SIZE (4096)
#define FLASH_SIZE (BLOCK_SIZE * 128)
static uint8_t flash[FLASH_SIZE];

static inline uint32_t addr(uint32_t block, uint32_t off) {
    return block * 4096 + off;
}

int user_provided_block_device_read(const struct lfs_config *c,
                                    lfs_block_t block,
                                    lfs_off_t off,
                                    void *buffer,
                                    lfs_size_t size)
{
    (void)c;

    uint32_t a = addr(block, off);
    memcpy(buffer, &flash[a], size);

    return 0;
}

int user_provided_block_device_prog(const struct lfs_config *c,
                                    lfs_block_t block,
                                    lfs_off_t off,
                                    const void *buffer,
                                    lfs_size_t size)
{
    (void)c;

    uint32_t a = addr(block, off);

    for (lfs_size_t i = 0; i < size; i++) {
        flash[a + i] &= ((uint8_t *)buffer)[i];
    }

    return 0;
}

int user_provided_block_device_erase(const struct lfs_config *c,
                                     lfs_block_t block)
{
    (void)c;

    uint32_t a = addr(block, 0);
    memset(&flash[a], 0xFF, 4096);

    return 0;
}

int user_provided_block_device_sync(const struct lfs_config *c)
{
    (void)c;
    return 0;
}

// configuration of the filesystem is provided by this struct
const struct lfs_config cfg = {
    // block device operations
    .read  = user_provided_block_device_read,
    .prog  = user_provided_block_device_prog,
    .erase = user_provided_block_device_erase,
    .sync  = user_provided_block_device_sync,

    // block device configuration
    .read_size = 16,
    .prog_size = 16,
    .block_size = BLOCK_SIZE,
    .block_count = 128,
    .cache_size = 16,
    .lookahead_size = 16,
    .block_cycles = 500,
};

int basic(uint8_t verbose)
{
    // mount the filesystem
    memset(flash, 0xFF, FLASH_SIZE);
    char fileName[16] = "testFile.txt";

    if (lfs_mount(&lfs, &cfg))
    {
        lfs_format(&lfs, &cfg);
        lfs_mount(&lfs, &cfg);
    }

    for (int i = 0; i < 10000; i++)
    {
        uint32_t boot_count = 0;

        lfs_file_open(&lfs, &file, fileName, LFS_O_RDWR | LFS_O_CREAT);
        lfs_file_read(&lfs, &file, &boot_count, sizeof(boot_count));

        boot_count++;

        lfs_file_rewind(&lfs, &file);
        lfs_file_write(&lfs, &file, &boot_count, sizeof(boot_count));
        lfs_file_close(&lfs, &file);

        if (verbose)
        {
            printf("boot_count=%u\n", boot_count);
        }
    }

    lfs_unmount(&lfs);
    return 0;
}


int multiple_files(uint8_t verbose)
{
    // mount the filesystem
    memset(flash, 0xFF, FLASH_SIZE);

    if (lfs_mount(&lfs, &cfg))
    {
        lfs_format(&lfs, &cfg);
        lfs_mount(&lfs, &cfg);
    }

    for (int i = 0; i < 100; i++)
    {
        char fileName[16];
        sprintf(fileName, "file%u.txt", i);

        lfs_file_open(&lfs, &file, fileName, LFS_O_RDWR | LFS_O_CREAT);
        lfs_file_write(&lfs, &file, fileName, strlen(fileName));
        lfs_file_close(&lfs, &file);

        if (verbose)
        {
            printf("Created file: %s\n", fileName);
        }
    }

    lfs_unmount(&lfs);
    return 0;
}


int main(void)
{
    if (basic(QUIET)) return 1;
    if (multiple_files(VERBOSE)) return 1;

    return 0;
}