# FUSE-over-SSD
使用FUSE文件系统对SSD进行操作

在Linux系统上使用FUSE文件系统并操作SSD设备（如`/dev/nvme0n1`），你可以按照以下步骤进行操作：

### 安装FUSE
首先，确保你的系统已经安装了FUSE。你可以使用包管理器来安装：

对于Ubuntu系统：
```bash
sudo apt-get update
sudo apt-get install fuse
```

### 确保内核模块已加载
确保FUSE模块已加载到内核中：
```bash
sudo modprobe fuse
```
### 查看安装的FUSE版本
```bash
fusermount -V
```

### 格式化并挂载SSD
假设你要使用FUSE文件系统来操作一个已经存在的文件系统或创建一个新的文件系统在SSD上，你需要先进行格式化并挂载：

#### 1. 格式化SSD（如果需要）
**注意**：这会清除设备上的所有数据。确保你已经备份了重要数据。
```bash
sudo mkfs.ext4 /dev/nvme0n1
```

#### 2. 创建挂载点
创建一个目录作为挂载点：
```bash
sudo mkdir /mnt/myssd
```

#### 3. 挂载SSD
挂载设备到刚才创建的挂载点：
```bash
sudo mount /dev/nvme0n1 /mnt/myssd
```

### 自定义FUSE文件系统
如果你要开发自定义的FUSE文件系统，可以参考以下步骤：

#### 1. 安装开发库
安装FUSE开发库：
```bash
sudo apt-get install libfuse-dev
```

#### 2. 编写FUSE程序
编写一个简单的FUSE程序。例如，创建一个名为`hellofs.c`的文件：
```c
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

static int hello_getattr(const char *path, struct stat *stbuf) {
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

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
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

static int hello_open(const char *path, struct fuse_file_info *fi) {
    if (find_file(path + 1) == -1) { // Skip leading '/'
        return -ENOENT;
    }
    return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
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

static int hello_write(const char *path, const char *buf, size_t size, off_t offset,
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

static int hello_unlink(const char *path) {
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

static struct fuse_operations hello_oper = {
    .getattr = hello_getattr,
    .readdir = hello_readdir,
    .open    = hello_open,
    .read    = hello_read,
    .write   = hello_write,
    .unlink  = hello_unlink,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &hello_oper, NULL);
}

```

编译FUSE程序：
```bash
gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
```

#### 3. 运行FUSE文件系统

运行FUSE文件系统：
```bash
./hello /mnt/myssd
```
此时会出现报这个错误：  

![image](https://github.com/lus-oa/FUSE-over-SSD/assets/122666739/388bd4b8-de7e-4913-a8b7-c84c7fbb6fb3)  

在后边加上`-o nonempty`参数：
```bash
./hello /mnt/myssd -o nonempty
```
之后会继续报错：  
![image](https://github.com/lus-oa/FUSE-over-SSD/assets/122666739/e0c6003e-4912-408c-992d-af61f8f075ea)  

这个错误表明当前用户没有对挂载点 `/mnt/myssd` 的写权限。为了挂载 FUSE 文件系统，用户需要对挂载点有写权限。

以下是解决这个问题的步骤：

##### 1. 确认当前用户
首先，确认你当前的用户。你可以通过以下命令查看当前用户：
```bash
whoami
```

##### 2. 修改挂载点的权限
确保当前用户对挂载点 `/mnt/myssd` 具有写权限。假设你的用户名是 `yourusername`，你可以使用以下命令来修改挂载点的所有权和权限：

```bash
sudo chown yourusername:yourusername /mnt/myssd
sudo chmod u+w /mnt/myssd
```

将 `yourusername` 替换为你的实际用户名。

###### 3. 挂载文件系统
然后尝试再次挂载 FUSE 文件系统：

```bash
./hello /mnt/myssd
```

或使用 `nonempty` 选项（如果目录非空）：

```bash
./hello /mnt/myssd -o nonempty
```

![image](https://github.com/lus-oa/FUSE-over-SSD/assets/122666739/5863aad4-5440-4a25-afc5-a75c6933804d)


通过这些步骤，你应该能够成功挂载 FUSE 文件系统到你的固态硬盘上。



现在，你可以在`/mnt/myssd`目录中看到一个虚拟的文件系统，包含一个`hello`文件。

这些步骤应帮助你在Linux系统上使用FUSE文件系统，并操作SSD设备`/dev/nvme0n1`。根据你的具体需求，你可能需要调整某些步骤或使用不同的FUSE文件系统。


### 具体操作
挂载成功后，你可以使用标准的文件操作命令来与FUSE文件系统进行交互。这些命令包括`ls`、`cd`、`cat`、`touch`、`echo`等。以下是一些常见操作的示例，假设你的FUSE文件系统挂载在`/mnt/myssd`：

#### 查看文件系统内容
列出挂载点中的文件和目录：
```bash
ls /mnt/myssd
```

#### 读取文件内容
查看文件内容（假设你在FUSE文件系统中有一个`hello`文件）：
```bash
cat /mnt/myssd/hello
```

#### 创建新文件
创建一个新的文件并写入内容：
```bash
echo "This is a test file" > /mnt/myssd/testfile
```

#### 读取新文件内容
读取刚才创建的文件内容：
```bash
cat /mnt/myssd/testfile
```

#### 删除文件
删除一个文件：
```bash
rm /mnt/myssd/testfile
```

#### 进入挂载点目录
进入挂载点目录并进行操作：
```bash
cd /mnt/myssd
```

#### 交互示例
假设你已经成功挂载了FUSE文件系统，并且挂载点是`/mnt/myssd`，可以执行以下操作：

```bash
# 列出文件
ls /mnt/myssd

# 查看hello文件内容
cat /mnt/myssd/hello

# 创建一个新文件
echo "This is a test file" > /mnt/myssd/testfile

# 读取新文件内容
cat /mnt/myssd/testfile

# 删除新文件
rm /mnt/myssd/testfile
```

### 特定于FUSE的选项和命令
除了标准文件操作命令，FUSE还提供了一些特定的选项和命令：

#### 1. 使用`fusermount`卸载文件系统
你可以使用`fusermount`命令来卸载FUSE文件系统：
```bash
fusermount -u /mnt/myssd
```

#### 2. 使用特定选项挂载
在挂载时可以指定一些FUSE特定的选项，比如`allow_other`允许其他用户访问挂载的文件系统：

```bash
./hello /mnt/myssd -o allow_other
```

#### 3. 调试信息
如果你在开发或调试FUSE文件系统，可以使用`-d`选项启动调试模式：

```bash
./hello /mnt/myssd -d
```

### 处理权限问题
如果在使用过程中遇到权限问题，可以尝试使用`sudo`命令或者调整文件系统的权限。

通过这些操作，你可以与挂载的FUSE文件系统进行交互，并根据需要执行各种文件操作。如果你有特定的FUSE文件系统实现（如SSHFS），它可能会提供更多特定的命令和选项。
