#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#define MAX_FILES 100
#define MAX_FILE_SIZE 1024

typedef struct {
    char name[256];
    char content[MAX_FILE_SIZE];
    size_t size;
} File;

File files[MAX_FILES];
int file_count = 0;

static int find_file(const char *name) {
    for (int i = 0; i < file_count; i++) {
        if (strcmp(files[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int lxc_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else {
        int idx = find_file(path + 1); // Skip leading '/'
        if (idx != -1) {
            stbuf->st_mode = S_IFREG | 0644;
            stbuf->st_nlink = 1;
            stbuf->st_size = files[idx].size;
        } else {
            return -ENOENT;
        }
    }
    return 0;
}

static int lxc_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    if (strcmp(path, "/") != 0) {
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    for (int i = 0; i < file_count; i++) {
        filler(buf, files[i].name, NULL, 0);
    }
    return 0;
}

static int lxc_open(const char *path, struct fuse_file_info *fi) {
    if (find_file(path + 1) == -1) { // Skip leading '/'
        return -ENOENT;
    }
    return 0;
}

static int lxc_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    int idx = find_file(path + 1); // Skip leading '/'
    if (idx == -1) {
        return -ENOENT;
    }

    if (offset < files[idx].size) {
        if (offset + size > files[idx].size) {
            size = files[idx].size - offset;
        }
        memcpy(buf, files[idx].content + offset, size);
    } else {
        size = 0;
    }
    return size;
}

static int lxc_write(const char *path, const char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    int idx = find_file(path + 1); // Skip leading '/'
    if (idx == -1) {
        if (file_count >= MAX_FILES) {
            return -ENOSPC;
        }
        idx = file_count++;
        strncpy(files[idx].name, path + 1, sizeof(files[idx].name) - 1);
        files[idx].name[sizeof(files[idx].name) - 1] = '\0';
        files[idx].size = 0;
    }

    if (offset + size > MAX_FILE_SIZE) {
        size = MAX_FILE_SIZE - offset;
    }

    memcpy(files[idx].content + offset, buf, size);
    if (offset + size > files[idx].size) {
        files[idx].size = offset + size;
    }
    return size;
}

static int lxc_unlink(const char *path) {
    int idx = find_file(path + 1); // Skip leading '/'
    if (idx == -1) {
        return -ENOENT;
    }

    for (int i = idx; i < file_count - 1; i++) {
        files[i] = files[i + 1];
    }
    file_count--;
    return 0;
}

static struct fuse_operations lxc_oper = {
    .getattr = lxc_getattr,
    .readdir = lxc_readdir,
    .open    = lxc_open,
    .read    = lxc_read,
    .write   = lxc_write,
    .unlink  = lxc_unlink,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &lxc_oper, NULL);
}

