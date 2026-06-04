#include "./external/littlefs/lfs.h"
#include <stdio.h>
#include <stdint.h>


// variables used by the filesystem
lfs_t lfs;
lfs_file_t file;

enum
{
    QUIET,
    VERBOSE,
};

int save_flash_to_bin(const uint8_t *flash,
                      size_t flash_size,
                      const char *filename)
{
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL)
    {
        perror("fopen");
        return -1;
    }

    size_t written = fwrite(flash, 1, flash_size, fp);

    fclose(fp);

    if (written != flash_size)
    {
        fprintf(stderr,
                "Failed to write complete file (%zu/%zu bytes)\n",
                written,
                flash_size);
        return -1;
    }

    return 0;
}


#define BLOCK_COUNT (128)
#define BLOCK_SIZE (4096)
#define FLASH_SIZE (BLOCK_SIZE * BLOCK_COUNT)
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
    .read_size = 1,
    .prog_size = 1,
    .block_size = BLOCK_SIZE,
    .block_count = BLOCK_COUNT,
    .cache_size = 512,
    .lookahead_size = 16,
    .compact_thresh = -1,
    .block_cycles = 50000,
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

#define MAX_FILES 995
int multiple_files(uint8_t verbose)
{
    // mount the filesystem
    memset(flash, 0xFF, FLASH_SIZE);

    if (lfs_mount(&lfs, &cfg))
    {
        lfs_format(&lfs, &cfg);
        lfs_mount(&lfs, &cfg);
    }

    for (int i = 0; i < MAX_FILES; i++)
    {
        char fileName[16];
        char content[100];
        sprintf(fileName, "file%u.txt", i);
        snprintf(content, sizeof(content), "Content of file %u", i);

        lfs_file_open(&lfs, &file, fileName, LFS_O_RDWR | LFS_O_CREAT);
        lfs_file_write(&lfs, &file, content, strlen(content));
        lfs_file_close(&lfs, &file);

        if (verbose)
        {
            printf("Created file: %s\n", fileName);
        }
    }
    
    for (int i = 0; i < MAX_FILES; i++)
    {
        char fileName[16];
        char readBuffer[100];
        sprintf(fileName, "file%u.txt", i);

        lfs_file_open(&lfs, &file, fileName, LFS_O_RDWR | LFS_O_CREAT);
        int bytes_read = lfs_file_read(&lfs, &file, readBuffer, sizeof(readBuffer) - 1);

        if (bytes_read >= 0 && verbose)
        {
            readBuffer[bytes_read] = '\0'; // Make it a C string
            printf("Content: %s\n", readBuffer);
        }

        lfs_file_close(&lfs, &file);
    }

    save_flash_to_bin(flash, FLASH_SIZE, "flash_dump.bin");
    lfs_unmount(&lfs);
    return 0;
}

void print_file_metadata(lfs_t *lfs, const char *path)
{
    struct lfs_info info;
    
    // Call lfs_stat
    int err = lfs_stat(lfs, path, &info);
    
    if (err < 0) {
        printf("Error getting status or file does not exist: %d\n", err);
        return;
    }
    
    // Process the metadata
    printf("Name: %s\n", info.name);
    if (info.type == LFS_TYPE_REG) {
        printf("Type: Regular File\n");
        printf("Size: %u bytes\n", (unsigned int)info.size);
    } else if (info.type == LFS_TYPE_DIR) {
        printf("Type: Directory\n");
    }
}

int file_metadata(uint8_t verbose)
{
    lfs_t locallfs;
    lfs_file_t localfile;
    memset(flash, 0xFF, FLASH_SIZE);
    char fileName[16] = "meta.txt";
    char content[5000] = {0xaa};

    if (lfs_mount(&locallfs, &cfg))
    {
        lfs_format(&locallfs, &cfg);
        lfs_mount(&locallfs, &cfg);
    }

    lfs_file_open(&locallfs, &localfile, fileName, LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_write(&locallfs, &localfile, &content[0], sizeof(content));
    lfs_file_close(&locallfs, &localfile);

    if (verbose)
    {
        print_file_metadata(&locallfs, fileName);
    }
}

int main(void)
{
    // choose between QUIET and VERBOSE modes
    if (basic(QUIET)) return 1;
    if (multiple_files(VERBOSE)) return 1;
    if (file_metadata(QUIET)) return 1;

    return 0;
}