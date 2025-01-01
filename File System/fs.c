#include "fs.h"
#include "disk.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>


#define MAX_FD 32
#define MAX_FILES 64
#define MAX_F_NAME 15

struct super_block
{
    int fat_idx;  // First block of the FAT
    int fat_len;  // Length of FAT in blocks
    int dir_idx;  // First block of directory
    int dir_len;  // Length of directory in blocks
    int data_idx; // First block of file-data
};

struct dir_entry
{
    int used;                  // Is this file-”slot” in use
    char name[MAX_F_NAME + 1]; // DOH!
    int size;                  // file size
    int head;                  // first data block of file
    int ref_cnt;
    // how many open file descriptors are there?
    // ref_cnt > 0 -> cannot delete file
};

struct file_descriptor
{
    int used; // fd in use
    int file; // the first block of the file
    // (f) to which fd refers too
    int offset; // position of fd within f
};

struct super_block fs;
struct file_descriptor fildesA[MAX_FD]; // 32
int *FAT;                               // Will be populated with the FAT data
struct dir_entry *DIR;                  // Will be populated with the directory data

/*This function creates a fresh (and empty) file system on the virtual disk with name disk_name.
As part of this function, you should first invoke make_disk(disk_name) to create a new disk.
Then, open this disk and write/initialize the necessary meta-information for your file system so
that it can be later used (mounted). The function returns 0 on success, and -1 if the disk
disk_name could not be created, opened, or properly initialized.*/
int make_fs(char *disk_name)
{
    if (make_disk(disk_name) < 0)
    {
        fprintf(stderr, "make_fs: Failed to create the disk '%s'.\n", disk_name);
        return -1;
    }
    if (open_disk(disk_name) < 0)
    {
        fprintf(stderr, "make_fs: Failed to open the disk '%s'.\n", disk_name);
        return -1;
    }
    fs.data_idx = DISK_BLOCKS / 2;

    fs.dir_idx = sizeof(fs) / BLOCK_SIZE + 1;
    

    fs.dir_len = (MAX_FILES * sizeof(struct dir_entry)) / BLOCK_SIZE + 1;

    fs.fat_idx = fs.dir_idx + fs.dir_len + 1;

    fs.fat_len = ((DISK_BLOCKS - fs.data_idx) * sizeof(int)) / BLOCK_SIZE + 1;

    // FAT = calloc(fs.fat_len * BLOCK_SIZE, 1);
    // DIR = calloc(fs.dir_len * BLOCK_SIZE, 1);

    char buffer[BLOCK_SIZE];
    memset(buffer, '\0', BLOCK_SIZE);

    memcpy(buffer, &fs, sizeof(struct super_block));
    if (block_write(0, buffer) != 0) // write super block to disk
        return -1;

    memset(buffer, 0, BLOCK_SIZE);
    int i;
    for (i = fs.dir_idx; i < fs.dir_idx + fs.dir_len; i++) // initialize the directory
    {
        if (block_write(i, buffer) != 0)
            return -1;
    }

    memset(buffer, '\0', BLOCK_SIZE);
    for (i = fs.fat_idx; i < fs.fat_idx + fs.fat_len; i++) // initialize the FAT
    {
        if (block_write(i, buffer) != 0)
            return -1;
    }

    if (close_disk() != 0)
        return -1; // close disk
    return 0;
}

/*This function mounts a file system that is stored on a virtual disk with name disk_name. With
the mount operation, a file system becomes "ready for use." You need to open the disk and
then load the meta-information that is necessary to handle the file system operations that are
discussed below. The function returns 0 on success, and -1 when the disk disk_name could not
be opened or when the disk does not contain a valid file system (that you previously created
with make_fs).
*/
int mount_fs(char *disk_name)
{
    if (open_disk(disk_name) != 0)
    {
        fprintf(stderr, "mount_fs: Failed to open the disk '%s'.\n", disk_name);
        return -1;
    }

    char buffer[BLOCK_SIZE];
    memset(buffer, '\0', BLOCK_SIZE);

    if (block_read(0, buffer) != 0) // load super block from disk
        return -1;
    memcpy(&fs, buffer, sizeof(struct super_block));

    FAT = (int *)malloc(fs.fat_len * BLOCK_SIZE);
    if (FAT == NULL)
        return -1;
    int i;
    for (i = 0; i < fs.fat_len; i++) // load FAT from disk
    {
        if (block_read(fs.fat_idx + i, buffer) != 0)
        {
            free(FAT);
            return -1;
        }
        memcpy(FAT + (i * (BLOCK_SIZE / sizeof(int))), buffer, BLOCK_SIZE); // copy buffer to FAT array indexes
    }

    DIR = (struct dir_entry *)malloc(fs.dir_len * BLOCK_SIZE);
    if (DIR == NULL)
    {
        return -1;
        free(FAT);
    }

    for (i = 0; i < fs.dir_len; i++) // load DIR from disk
    {
        if (block_read(fs.dir_idx + i, buffer) != 0)
        {
            free(FAT);
            free(DIR);
            return -1;
        }
        memcpy((char *)DIR + (i * BLOCK_SIZE), buffer, BLOCK_SIZE); // copy buffer to DIR array indexes
    }
    memset(fildesA, 0, sizeof(fildesA)); // initialize fds
    // for (i = 0; i < MAX_FD; i++) {
    //     fildesA[i].used = 0;
    // }
    return 0;
}

int umount_fs(char *disk_name)
{

    char buffer[BLOCK_SIZE];
    memset(buffer, '\0', BLOCK_SIZE);
    int i;
    if (FAT != NULL) {
        for (i = 0; i < fs.fat_len; i++) {
            memcpy(buffer, FAT + (i * (BLOCK_SIZE / sizeof(int))), BLOCK_SIZE);
            if (block_write(fs.fat_idx + i, buffer) != 0)
                return -1;
        }
        free(FAT);
        FAT = NULL;
    }

    if (DIR != NULL) {
        for (i = 0; i < fs.dir_len; i++) {
            memcpy(buffer, (char *)DIR + (i * BLOCK_SIZE), BLOCK_SIZE);
            if (block_write(fs.dir_idx + i, buffer) != 0)
                return -1;
        }
        free(DIR);
        DIR = NULL;
    }

    for (i = 0; i < MAX_FD; i++)
        fs_close(i); // Close file descriptors

    if (close_disk() < 0) {
        fprintf(stderr, "unmount_fs: Failed to close the disk.\n");
        return -1;
    }

    return 0;
}


int fs_open(char *name)
{
    int filenum = -1;
    int i;
    for (i = 0; i < MAX_FILES; i++)
    {
        if (DIR[i].used && strcmp(DIR[i].name, name) == 0) // check if file exists in DIR
        {
            filenum = i;
            break;
        }
    }
    if (filenum == -1)
    {
        fprintf(stderr, "fs_open: File '%s' not found.\n", name);
        return -1;
    }
    
    int fdnum = -1;
    for (i = 0; i < MAX_FD; i++)
    {
        if (!fildesA[i].used) // find an unused fd
        {
            fdnum = i;
            fildesA[i].used = 1;
            fildesA[i].file = filenum;
            fildesA[i].offset = 0;
            break;
        }
    }
    if (fdnum == -1)
    {
        fprintf(stderr, "fs_open: No available file descriptors.\n");
        return -1;
    }
    DIR[filenum].ref_cnt++;
    return fdnum;
}

int fs_close(int fildes)
{
    if (fildes < 0 || fildes >= MAX_FD || !fildesA[fildes].used)
    {
        fprintf(stderr, "fs_close: Invalid file descriptor.\n");
        return -1;
    }

    fildesA[fildes].used = 0;
    fildesA[fildes].file = -1;
    fildesA[fildes].offset = 0;

    if (DIR[fildesA[fildes].file].used)
        DIR[fildesA[fildes].file].ref_cnt--;

    return 0;
}

int fs_create(char *name)
{

    if (strlen(name) > MAX_F_NAME)
    {
        fprintf(stderr, "fs_create: File name too long.\n");
        return -1;
    }

    int i;
    for (i = 0; i < MAX_FILES; i++)
    {
        if (memcmp(DIR[i].name, name, strlen(name)) == 0)
        {
            fprintf(stderr, "fs_create: File '%s' already exists.\n", name);
            return -1;
        }
    }

    int freeIND = -1;
    for (i = 0; i < MAX_FILES; i++) // find a free slot
    {
        if (DIR[i].used == 0)
        {
            freeIND = i;
            break;
        }
    }
    if (freeIND == -1)
    {
        fprintf(stderr, "fs_create: No free slots in the directory.\n");
        return -1;
    }

    DIR[freeIND].used = 1;
    DIR[freeIND].size = 0;
    DIR[freeIND].head = -1;
    DIR[freeIND].ref_cnt = 0;
    memcpy(DIR[freeIND].name, name, strlen(name));
    // DIR[free_idx].name[MAX_F_NAME] = '\0';

    return 0;
}

int fs_delete(char *name)
{
    int i;
    for (i = 0; i < MAX_FILES; i++)
    {
        if (DIR[i].used && strcmp(DIR[i].name, name) == 0)
        {
            if (DIR[i].ref_cnt > 0)
            {
                fprintf(stderr, "fs_delete: File '%s' is open.\n", name);
                return -1;
            }

            int block = DIR[i].head;
            while (block != -1) // free all blocks in the file
            {
                int nextblock = FAT[block];
                FAT[block] = 0;
                block = nextblock;
            }

            DIR[i].used = 0;
            DIR[i].size = 0;
            DIR[i].head = -1;
            DIR[i].ref_cnt = 0;
            memset(DIR[i].name, '\0', strlen(name));

            return 0;
        }
    }
    return -1;
}

int fs_read(int fildes, void *buf, size_t nbyte)
{
    // Validate file descriptor
    if (fildes < 0 || fildes >= MAX_FD || !fildesA[fildes].used)
    {
        fprintf(stderr, "fs_read: Invalid file descriptor.\n");
        return -1;
    }
    struct file_descriptor *fd = &fildesA[fildes];
    struct dir_entry *file = &DIR[fd->file];

    // Handle case when offset is already at the end of the file, no more data
    if (fd->offset >= file->size)
    {
        return 0;
    }

    size_t totalbytes = nbyte;
    if (fd->offset + nbyte > file->size)
        totalbytes = file->size - fd->offset; // Adjust bytes to read if reaching EOF

    int current_block = file->head;
    size_t offset_in_block = fd->offset % BLOCK_SIZE;
    size_t block_offset = fd->offset / BLOCK_SIZE;

    // Traverse FAT to find the correct starting block
    while (block_offset >= BLOCK_SIZE && current_block != -1)
    {
        current_block = FAT[current_block]; // Move to the next block
        block_offset--;
    }

    size_t bytes_read = 0;
   

    // Read the data block-by-block
    while (totalbytes > 0 && current_block != -1)
    { 
        char block_data[BLOCK_SIZE];
        if (block_read(fs.data_idx + current_block, block_data) != 0)
        {
            fprintf(stderr, "fs_read: Failed to read block from disk.\n");
            return -1;
        }
        size_t bytes_from_block = BLOCK_SIZE - offset_in_block;
        if (bytes_from_block > totalbytes)
        {
            bytes_from_block = totalbytes;
        }

        memcpy((char *)buf + bytes_read, block_data + offset_in_block, bytes_from_block);
        totalbytes -= bytes_from_block;
        bytes_read += bytes_from_block;
        offset_in_block = 0;

        current_block = FAT[current_block];
    }

    fd->offset += bytes_read;

    return bytes_read;
}

int fs_write(int fildes, void *buf, size_t nbyte)
{
    // Validate file descriptor
    if (fildes < 0 || fildes >= MAX_FD || !fildesA[fildes].used)
    {
        fprintf(stderr, "fs_read: Invalid file descriptor.\n");
        return -1;
    }
    struct file_descriptor *fd = &fildesA[fildes];
    struct dir_entry *file = &DIR[fd->file];

    size_t bytes_written = 0;
    size_t remaining_bytes = nbyte;
    size_t offset_in_block = fd->offset%BLOCK_SIZE;

    int current_block = file->head;

    while (offset_in_block >= BLOCK_SIZE)
    {
        if (current_block == -1) break;
        current_block = FAT[current_block];
        offset_in_block -= BLOCK_SIZE;
    }

    int i;

    // Write data block by block
    while (remaining_bytes > 0)
    {
        if (current_block == -1) {
            for (i = 0; i < DISK_BLOCKS; i++) {
                if (FAT[i] == 0) {
                    FAT[i] = -1;
                    if (file->head == -1) file->head = i;
                    else {
                        int last_block = file->head;
                        while (FAT[last_block] != -1) last_block = FAT[last_block];
                        FAT[last_block] = i;
                    }
                    current_block = i;
                    break;
                }
            }
            if (current_block == -1) {
                fprintf(stderr, "fs_write: No space left on disk.\n");
                return bytes_written;
            }
        }

        char block_data[BLOCK_SIZE];
        if (block_read(fs.data_idx + current_block, block_data) != 0) {
            memset(block_data, 0, BLOCK_SIZE); // Initialize new block
        }

        size_t bytes_in_block = BLOCK_SIZE - offset_in_block;
        if (bytes_in_block > remaining_bytes) bytes_in_block = remaining_bytes;

        memcpy(block_data + offset_in_block, (char *)buf + bytes_written, bytes_in_block);

        if (block_write(fs.data_idx + current_block, block_data) != 0) {
            fprintf(stderr, "fs_write: Failed to write block to disk.\n");
            return -1;
        }

        bytes_written += bytes_in_block;
        remaining_bytes -= bytes_in_block;
        offset_in_block = 0;

        // Allocate next block if needed
        if (remaining_bytes > 0) {
            if (FAT[current_block] == -1) {
                for (i = 0; i < DISK_BLOCKS; i++) {
                    if (FAT[i] == 0) {
                        FAT[current_block] = i;
                        FAT[i] = -1;
                        current_block = i;
                        break;
                    }
                }
                if (FAT[current_block] == -1) {
                    fprintf(stderr, "fs_write: No space left on disk.\n");
                    return bytes_written;
                }
            }
            current_block = FAT[current_block];
        }
    }

    // Update file size and offset
    fd->offset += bytes_written;
    if (fd->offset > file->size)
        file->size = fd->offset;

    return bytes_written;
}

int fs_get_filesize(int fildes)
{
    // Validate file descriptor
    if (fildes < 0 || fildes >= MAX_FD || !fildesA[fildes].used)
    {
        fprintf(stderr, "fs_get_filesize: Invalid file descriptor.\n");
        return -1;
    }
    struct file_descriptor *fd = &fildesA[fildes];
    struct dir_entry *file = &DIR[fd->file];

    return file->size;
}

int fs_listfiles(char ***files)
{
    int i;
    int count = 0;
    for (i = 0; i < MAX_FILES; i++) // count the number of files
    {
        if (DIR[i].used)
        {
            count++;
        }
    }

    char **file_array = (char **)malloc((count + 1) * sizeof(char *)); //+ 1 for NULL
    if (file_array == NULL)
    {
        return -1;
    }

    int index = 0;
    for (i = 0; i < MAX_FILES; i++)
    {
        if (DIR[i].used)
        {
            file_array[index] = (char *)malloc(strlen(DIR[i].name) + 1);
            strcpy(file_array[index], DIR[i].name);
            index++;
        }
    }

    file_array[index] = NULL;
    *files = file_array;

    return 0;
}

int fs_lseek(int fildes, off_t offset)
{
    // Validate file descriptor
    if (fildes < 0 || fildes >= MAX_FD || !fildesA[fildes].used)
    {
        fprintf(stderr, "fs_lseek: Invalid file descriptor.\n");
        return -1;
    }
    struct file_descriptor *fd = &fildesA[fildes];
    struct dir_entry *file = &DIR[fd->file];

    if (offset < 0 || offset > file->size)
    {
        fprintf(stderr, "fs_lseek: Invalid offset.\n");
        return -1;
    }

    fd->offset = offset;

    return 0;
}

int fs_truncate(int fildes, off_t length)
{
    // Validate file descriptor
    if (fildes < 0 || fildes >= MAX_FD || !fildesA[fildes].used)
    {
        fprintf(stderr, "fs_read: Invalid file descriptor.\n");
        return -1;
    }

    struct file_descriptor *fd = &fildesA[fildes];
    struct dir_entry *file = &DIR[fd->file];

    // Validate the length
    if (length > file->size)
    {
        fprintf(stderr, "fs_truncate: Invalid length.\n");
        return -1;
    }

    // Update the file pointer if it is beyond the new file length
    if (fd->offset > length)
    {
        fd->offset = length;
    }

    int current_block = file->head;
    size_t offset_in_block = length;

    // Traverse to the block corresponding to the new file length
    while (offset_in_block >= BLOCK_SIZE && current_block != -1)
    {
        current_block = FAT[current_block];
        offset_in_block -= BLOCK_SIZE;
    }

    // Free any blocks after the truncation point
    if (current_block != -1)
    {
        int next_block = FAT[current_block];
        FAT[current_block] = -1; // Terminate the file at this block

        while (next_block != -1)
        {
            int temp_block = next_block;
            next_block = FAT[next_block];
            FAT[temp_block] = 0; // Free the block
        }
    }

    file->size = length;

    return 0;
}
