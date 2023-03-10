#define FUSE_USE_VERSION 30

#include <errno.h>
#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>


struct node {
    struct node *next;
    struct node *child; // only for directory

    int is_file;
    char *name;
    char *content;

    // attributes
    uid_t uid;
    gid_t gid;
    time_t access_time;
    time_t modification_time;
};

static struct node *root;

struct node *find_node(const char *const_path)
{
    if (strcmp(const_path, "/") == 0) {
        return root;
    }

    // copy `const_path` to `path`
    int len = strlen(const_path) + 1;
    char *path = (char *) malloc(sizeof(char) * (len));
    strcpy(path, const_path);
    path[len-1] = '\0';

    // traverse the tree from root to find target node
    struct node *now = root;
    while (strlen(path) > 0) {
        if (now->is_file) {
            break;
        }

        // strip `/`
        int i = 0;
        while (path[i++] == '/') {
            path = path + 1;
        }

        // get name
        int count = 0;
        while (path[count] != '/' && path[count] != '\0') {
            count++;
        }
        char *name = (char *) malloc(sizeof(char) * (count+1));
        strncpy(name, path, count);
        name[count] = '\0';
        path = path + count;

        struct node *cur = now->child;
        while (cur != NULL) {
            if (strcmp(cur->name, name) == 0) {
                now = cur;
                break;
            }
            cur = cur->next;
        }
        if (cur == NULL) {  // not found
            return NULL;
        }
    }
    return now;
}

struct node *find_prev_node(const char *const_path)
{
    if (strcmp(const_path, "/") == 0) {
        return root;
    }

    // copy `const_path` to `path`
    int len = strlen(const_path) + 1;
    char *path = (char *) malloc(sizeof(char) * (len));
    strcpy(path, const_path);
    path[len-1] = '\0';

    // traverse the tree from root to find target node
    struct node *now = root;
    struct node *prev;
    while (strlen(path) > 0) {
        if (now->is_file) {
            break;
        }

        // strip `/`
        int i = 0;
        while (path[i++] == '/') {
            path = path + 1;
        }

        // get name
        int count = 0;
        while (path[count] != '/' && path[count] != '\0') {
            count++;
        }
        char *name = (char *) malloc(sizeof(char) * (count+1));
        strncpy(name, path, count);
        name[count] = '\0';
        path = path + count;

        struct node *cur = now->child;
        prev = now;
        while (cur != NULL) {
            if (strcmp(cur->name, name) == 0) {
                now = cur;
                break;
            }
            prev = cur;
            cur = cur->next;
        }
        if (cur == NULL) {  // not found
            return NULL;
        }
    }
    return prev;
}

void set_stat(struct stat *st, struct node *target)
{
    // set struct stat
    st->st_uid = target->uid;
    st->st_gid = target->gid;
    st->st_atime = target->access_time;
    st->st_mtime = target->modification_time;

    if (target->is_file) {
        st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_size = 1024;
    } else {
        st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
    }
}

char *get_dirname(char *full_path)
{
    int len = strlen(full_path);
    char *dirname;

    for (int i = len-1; i >= 0; i--) {
        if (full_path[i] == '/') {
            dirname = (char *) malloc(sizeof(char) * (i+1));
            strncpy(dirname, full_path, i);

            if (strcmp(dirname, "") == 0) {
                return "/";
            }
            return dirname;
        }
    }
    printf("failed to get_dirname: %s\n", full_path);
    return NULL;
}

char *get_name(char *full_path)
{
    int len = strlen(full_path);
    char *name;

    for (int i = len-1; i >= 0; i--) {
        if (full_path[i] == '/') {
            name = (char *) malloc(sizeof(char) * (len-i));
            strncpy(name, full_path+i+1, len-i-1);
            return name;
        }
    }
    printf("failed to get_name: %s\n", full_path);
    return NULL;
}

struct node *create_dir_node(const char *name)
{
    struct node *dir = (struct node *) malloc(sizeof(struct node));

    dir->child = NULL;
    dir->next = NULL;

    dir->is_file = 0;
    // TODO: is direct assign a good idea? or we should copy then assign?
    dir->name = name;
    dir->content = "";

    time_t now = time(0);
    dir->access_time = now;
    dir->modification_time = now;

    dir->uid = getuid();
    dir->gid = getgid();

    return dir;
}

struct node *create_file_node(const char *name, const char *content)
{
    struct node *file = (struct node *) malloc(sizeof(struct node));

    file->child = NULL;
    file->next = NULL;

    file->is_file = 1;
    // TODO: is direct assign a good idea? or we should copy then assign?
    file->name = name;
    file->content = content;

    time_t now = time(0);
    file->access_time = now;
    file->modification_time = now;

    file->uid = getuid();
    file->gid = getgid();

    return file;
}

/**
 * My File Operations (Callbacks)
*/

static int my_getattr(const char *path, struct stat *st)
{
    printf("my_getattr path: %s\n", path);

    struct node *target = find_node(path);
    if (target == NULL) {
        return -ENOENT;
    }
    set_stat(st, target);
    return 0;
}

static int my_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    printf("my_readdir path: %s\n", path);

    // Current and Parent Directory, leave stat NULL for now
    filler(buffer, ".", NULL, 0);
    filler(buffer, "..", NULL, 0);

    struct node *target = find_node(path);
    if (target->is_file) {
        printf("node %s is a file, cannot perform readdir on file.\n", target->name);
        return -1;
    }

    struct node *cur = target->child;
    while (cur != NULL) {
        struct stat *st = (struct stat *) malloc(sizeof(struct stat));
        set_stat(st, cur);
        filler(buffer, cur->name, st, 0);
        cur = cur->next;
    }
    return 0;
}

static int my_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("my_read path: %s\n", path);

    struct node *target = find_node(path);

    char *content = target->content;
    memcpy(buffer, content + offset, size);
    return strlen(content) - offset;
}

static int my_mkdir(const char *path, mode_t mode)
{
    printf("my_mkdir path: %s\n", path);

    char *dirname = get_dirname((char *) path);
    struct node *dirname_node = find_node(dirname);

    // create mkdir dir node
    char *name = get_name((char *) path);
    struct node *dir = create_dir_node(name);

    struct node *cur = dirname_node->child;
    if (cur == NULL) {
        dirname_node->child = dir;
        return 0;
    }

    while (cur->next != NULL) {
        cur = cur->next;
    }
    cur->next = dir;
    return 0;
}

static int my_mknod(const char *path, mode_t mode, dev_t rdev)
{
    printf("my_mknod path: %s\n", path);

    char *dirname = get_dirname((char *) path);
    struct node *dirname_node = find_node(dirname);

    // create mknod file node
    char *name = get_name((char *) path);
    struct node *file = create_file_node(name, "");

    struct node *cur = dirname_node->child;
    if (cur == NULL) {
        dirname_node->child = file;
        return 0;
    }

    while (cur->next != NULL) {
        cur = cur->next;
    }
    cur->next = file;
    return 0;
}

static int my_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *info)
{
    printf("my_write path: %s\n", path);

    struct node *target = find_node(path);

    int len = strlen(target->content) + strlen(buffer) + 1;
    char *content = (char *) malloc(sizeof(char) * len);
    strcpy(content, target->content);
    strcpy(content + strlen(target->content), buffer);
    content[len-1] = '\0';

    target->content = content;
    return size;
}

static int my_unlink(const char *path)
{
    printf("my_unlink path: %s\n", path);

    struct node *prev_target = find_prev_node(path);
    if (prev_target == NULL) {
        printf("target: %s not exist\n", path);
        return -1;
    }

    struct node *target;
    char *name = get_name(path);
    if (prev_target->child && strcmp(prev_target->child->name, name) == 0) {
        target = prev_target->child;
        prev_target->child = target->next;
    } else if (prev_target->next && strcmp(prev_target->next->name, name) == 0) {
        target = prev_target->next;
        prev_target->next = target->next;
    } else {
        return -1;
    }
    free(target);
}

static int my_rmdir(const char *path)
{
    printf("my_rmdir path: %s\n", path);

    struct node *prev_target = find_prev_node(path);
    if (prev_target == NULL) {
        printf("target: %s not exist\n", path);
        return -1;
    }

    struct node *target;
    char *name = get_name(path);
    if (prev_target->child && strcmp(prev_target->child->name, name) == 0) {
        target = prev_target->child;
        prev_target->child = target->next;
    } else if (prev_target->next && strcmp(prev_target->next->name, name) == 0) {
        target = prev_target->next;
        prev_target->next = target->next;
    } else {
        return -1;
    }
    free(target);
}

static int my_open(const char *path, struct fuse_file_info *fi)
{
    printf("my_open path: %s\n", path);
    return 0;
}

static int my_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi)
{
    printf("my_utimens path: %s\n", path);
    return 0;
}

static struct fuse_operations operations = {
    .getattr = my_getattr,
    .readdir = my_readdir,
    .read    = my_read,
    .mkdir   = my_mkdir,
    .mknod   = my_mknod,
    .write   = my_write,
    .unlink  = my_unlink,
    .rmdir   = my_rmdir,
    .open    = my_open,
    .utimens = my_utimens,
    // .opendir
    // .create
};

int main(int argc, char *argv[])
{
    // create root node
    time_t now = time(0);
    root = (struct node *) malloc(sizeof(struct node));
    root->child = NULL;
    root->next = NULL;
    root->is_file = 0;
    root->name = "";
    root->content = "";
    root->access_time = now;
    root->modification_time = now;
    root->uid = getuid();
    root->gid = getgid();

    return fuse_main(argc, argv, &operations, NULL);
}
