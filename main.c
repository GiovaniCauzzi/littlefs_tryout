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

#define BLOCK_COUNT     512
#define BLOCK_SIZE      4096
#define FLASH_SIZE      BLOCK_SIZE * BLOCK_COUNT
#define WEAR_LEVELING   100e3

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
    .cache_size = 64,
    .lookahead_size = BLOCK_COUNT/8,
    // .compact_thresh = -1,
    .block_cycles = WEAR_LEVELING,
};

static void print_dir_recursive(lfs_t *lfs,
                                const char *path,
                                int depth)
{
    lfs_dir_t dir;
    struct lfs_info info;

    char fullpath[512];

    int err = lfs_dir_open(lfs, &dir, path);
    if (err < 0)
    {
        printf("%*s[ERR] cannot open %s (%d)\n",
               depth * 2, "", path, err);
        return;
    }

    while (true)
    {
        int res = lfs_dir_read(lfs, &dir, &info);
        if (res < 0)
        {
            printf("%*s[ERR] read failed (%d)\n",
                   depth * 2, "", res);
            break;
        }

        if (res == 0)
        {
            break; // end of directory
        }

        // Skip "." and ".."
        if (strcmp(info.name, ".") == 0 ||
            strcmp(info.name, "..") == 0)
        {
            continue;
        }

        printf("%*s%s",
               depth * 2, "",
               info.name);

        if (info.type == LFS_TYPE_DIR)
        {
            printf("/\n");

            // build next path
            if (strcmp(path, "/") == 0)
            {
                snprintf(fullpath, sizeof(fullpath),
                         "/%s", info.name);
            }
            else
            {
                snprintf(fullpath, sizeof(fullpath),
                         "%s/%s", path, info.name);
            }

            print_dir_recursive(lfs, fullpath, depth + 1);
        }
        else
        {
            printf(" (%lu bytes)\n",
                   (unsigned long)info.size);
        }
    }

    lfs_dir_close(lfs, &dir);
}

void lfs_print_tree(lfs_t *lfs)
{
    printf("LittleFS tree:\n");
    print_dir_recursive(lfs, "/", 0);
}

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

#define MAX_FILES 1000
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
    return 0;
}

void print_lfs_info(struct lfs_config *cfg)
{
    printf("LittleFS configuration\n");
    printf("Flash size     : %u KB\n",
           (cfg->block_size * cfg->block_count) / 1024);
    printf("Block size     : %u\n", cfg->block_size);
    printf("Block count    : %u\n", cfg->block_count);
    printf("Cache size     : %u\n", cfg->cache_size);
    printf("Lookahead size : %u\n", cfg->lookahead_size);
}

typedef struct
{
    uint8_t someFlag;
    uint32_t magic;
    uint32_t version;
    char someString[32];
} config_header_t;

int simulate_binary_config_file(uint8_t verbose)
{
    lfs_t locallfs;
    lfs_file_t localfile;
    memset(flash, 0xFF, FLASH_SIZE);
    char fileName[16] = "config.bin";
    config_header_t config = {
        .someFlag = 0xAB,
        .magic = 0xDEADBEEF,
        .version = 1,
        .someString = "Hello, LittleFS!"};
    config_header_t configLoad;

    if (verbose) printf("Simulating binary config file storage and retrieval...\n");

    if (lfs_mount(&locallfs, &cfg))
    {
        lfs_format(&locallfs, &cfg);
        lfs_mount(&locallfs, &cfg);
    }

    uint32_t counter = 100;
    while (counter--)
    { // Store config file in memory, simulating a embedded device config
        if (lfs_file_open(&locallfs, &localfile, fileName, LFS_O_RDWR | LFS_O_CREAT))
            return 1;
        if (lfs_file_write(&locallfs, &localfile, &config, sizeof(config)) < 0)
            return 1;
        if (lfs_file_sync(&locallfs, &localfile) < 0)
            return 1;
        if (lfs_file_close(&locallfs, &localfile) < 0)
            return 1;

        // Load config file from memory and compare with original
        if (lfs_file_open(&locallfs, &localfile, fileName, LFS_O_RDONLY) < 0)
            return 1;
        if (lfs_file_read(&locallfs, &localfile, &configLoad, sizeof(configLoad)) < 0)
            return 1;
        if (lfs_file_close(&locallfs, &localfile) < 0)
            return 1;
    }

    if (memcmp(&config, &configLoad, sizeof(config)) != 0)
    {
        printf("Error: Loaded config does not match saved config!\n");
        return -1;
    }

    if (verbose)
    {
        printf("Written binary config file: %s\n", fileName);
        printf("Config contents:\n");
        printf("  someFlag : 0x%02X\n", configLoad.someFlag);
        printf("  magic     : 0x%08X\n", configLoad.magic);
        printf("  version   : %u\n", configLoad.version);
        printf("  someString: %s\n", configLoad.someString);
    }
    save_flash_to_bin(flash, FLASH_SIZE, "config.bin");
    return 0;
}



int explore_folders(uint8_t verbose)
{
    lfs_t locallfs;
    lfs_file_t localfile;
    memset(flash, 0xFF, FLASH_SIZE);
    char fileName[16] = "config.bin";
    char content[16] = "someContent";
    char contentLoad[16];

    if (verbose) printf("Exploring folder creation and file storage within folders...\n");
    
    if (lfs_mount(&locallfs, &cfg))
    {
        lfs_format(&locallfs, &cfg);
        lfs_mount(&locallfs, &cfg);
    }

    if (lfs_mkdir(&locallfs, "/config") < 0)
    {
        if (verbose) printf("Failed to create directory\n");
        return 1;
    }

    if (lfs_mkdir(&locallfs, "/config/config1") < 0)
    {
        if (verbose) printf("Failed to create directory\n");
        return 1;
    }

    if (lfs_mkdir(&locallfs, "/config/config1/foo") < 0)
    {
        if (verbose) printf("Failed to create directory\n");
        return 1;
    }
    if (lfs_mkdir(&locallfs, "/config/config1/bar") < 0)
    {
        if (verbose) printf("Failed to create directory\n");
        return 1;
    }

    if (lfs_mkdir(&locallfs, "/config/config2") < 0)
    {
        if (verbose) printf("Failed to create directory\n");
        return 1;
    }
    // it really fails if trying to create again
    if (lfs_mkdir(&locallfs, "/config/config2"))
    {
        if (verbose) printf("Failed to detect existing directory\n");
        return 1;
    }

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", "/config", fileName);
    if (verbose) printf("Creating file at path: %s\n", path);

    if (lfs_file_open(&locallfs, &localfile, path, LFS_O_RDWR | LFS_O_CREAT) < 0)
        return 1;
    if (lfs_file_write(&locallfs, &localfile, &content, sizeof(content)) < 0)
        return 1;
    if (lfs_file_sync(&locallfs, &localfile) < 0)
        return 1;
    if (lfs_file_close(&locallfs, &localfile) < 0)
        return 1;

    if (lfs_file_open(&locallfs, &localfile, path, LFS_O_RDONLY) < 0)
        return 1;
    if (lfs_file_read(&locallfs, &localfile, &contentLoad, sizeof(contentLoad)) < 0)
        return 1;
    if (lfs_file_close(&locallfs, &localfile) < 0)
        return 1;

    if (memcmp(content, contentLoad, sizeof(content)) != 0)
    {
        if (verbose) printf("Error: Loaded content does not match saved content!\n");
        return -1;
    }   

    if (verbose) lfs_print_tree(&locallfs);
    return 0;
}

int main(void)
{
    // choose between QUIET and VERBOSE modes
    print_lfs_info((struct lfs_config *)&cfg);
    if (basic(QUIET))
        return 1;
    if (multiple_files(QUIET))
        return 1;
    if (file_metadata(QUIET))
        return 1;
    if (simulate_binary_config_file(QUIET))
        return 1;
    if (explore_folders(VERBOSE))
        return 1;

    return 0;
}