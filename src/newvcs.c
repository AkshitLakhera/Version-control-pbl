#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>

#define VCS_DIR ".myvcs"
#define OBJECTS_DIR ".myvcs/objects"
#define INDEX_FILE ".myvcs/index"
#define LOG_FILE ".myvcs/log"
#define HEAD_FILE ".myvcs/HEAD"
#define COMMIT_FILE ".myvcs/commit_id"
#define BRANCHES_DIR ".myvcs/branches"
#define BRANCH_HEADS ".myvcs/branch_heads"

#define HASH_SIZE 41
#define MAX_PATH_LEN 256

#define COLOR_RED "\033[0;31m"
#define COLOR_GREEN "\033[0;32m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_RESET "\033[0m"

/* Commit doubly linked list Structure (doubly linked list ) */
typedef struct CommitNode {
    char id[64];
    char message[256];
    struct CommitNode *parent;
    struct CommitNode **children;
    int child_count;
} CommitNode;

/* Commit Graph Edge List (Graph) */
typedef struct GraphEdge {
    char from[64];
    char to[64];
    struct GraphEdge *next;
} GraphEdge;

CommitNode *commit_tree_root = NULL; // Tree root
GraphEdge *commit_graph = NULL;      // Graph edge list

void add_commit_edge(const char *from, const char *to) {
    GraphEdge *edge = (GraphEdge *)malloc(sizeof(GraphEdge));
    strcpy(edge->from, from);
    strcpy(edge->to, to);
    edge->next = commit_graph;
    commit_graph = edge;
}

void get_current_branch(char *branch) {
    FILE *f = fopen(HEAD_FILE, "r");
    if (f) {
        fgets(branch, MAX_PATH_LEN, f);
        branch[strcspn(branch, "\n")] = 0;
        fclose(f);
    } else {
        strcpy(branch, "master");
    }
}

void get_branch_log_path(char *path) {
    char branch[MAX_PATH_LEN];
    get_current_branch(branch);
    snprintf(path, MAX_PATH_LEN, "%s/%s.log", BRANCHES_DIR, branch);
}

void simple_hash_file(const char *filename, char *output) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        strcpy(output, "0000000000000000000000000000000000000000");
        return;
    }

    unsigned long hash = 5381;
    int c;
    while ((c = fgetc(file)) != EOF) {
        hash = ((hash << 5) + hash) + c;
    }
    fclose(file);
    sprintf(output, "%040lx", hash);
}

void copy_file(const char *src, const char *dest) {
    FILE *fsrc = fopen(src, "rb");
    FILE *fdest = fopen(dest, "wb");
    if (!fsrc || !fdest) {
        if (fsrc) fclose(fsrc);
        if (fdest) fclose(fdest);
        return;
    }
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
        fwrite(buf, 1, n, fdest);
    }
    fclose(fsrc);
    fclose(fdest);
}

void init_repo() {
    if (mkdir(VCS_DIR, 0755) == 0) {
        mkdir(OBJECTS_DIR, 0755);
        mkdir(BRANCHES_DIR, 0755);
        mkdir(BRANCH_HEADS, 0755);

        FILE *f = fopen(INDEX_FILE, "w");
        if (f) fclose(f);
        f = fopen(HEAD_FILE, "w");
        if (f) {
            fprintf(f, "master");
            fclose(f);
        }

        char log_path[MAX_PATH_LEN];
        get_branch_log_path(log_path);
        f = fopen(log_path, "w");
        if (f) fclose(f);

        snprintf(log_path, MAX_PATH_LEN, "%s/master.txt", BRANCH_HEADS);
        f = fopen(log_path, "w");
        if (f) fclose(f);

        printf("Repository initialized with master branch.\n");
    } else {
        printf("Repository already exists.\n");
    }
}

void add_file(const char *filename) {
    FILE *index = fopen(INDEX_FILE, "a");
    if (!index) return;
    fprintf(index, "%s\n", filename);
    fclose(index);
    printf("Added '%s' to staging.\n", filename);
}

void write_object(const char *filename, const char *hash) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s", OBJECTS_DIR, hash);
    if (access(path, F_OK) == 0) return;

    FILE *src = fopen(filename, "rb");
    FILE *dest = fopen(path, "wb");
    if (!src || !dest) return;

    char buffer[1024];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, n, dest);
    }

    fclose(src);
    fclose(dest);
}

CommitNode *create_commit_node(const char *id, const char *message, CommitNode *parent) {
    CommitNode *node = (CommitNode *)malloc(sizeof(CommitNode));
    strcpy(node->id, id);
    strcpy(node->message, message);
    node->parent = parent;
    node->children = NULL;
    node->child_count = 0;
    if (parent) {
        parent->children = realloc(parent->children, sizeof(CommitNode *) * (parent->child_count + 1));
        parent->children[parent->child_count++] = node;
    }
    return node;
}

void commit(const char *message) {
    char commit_id[64];
    time_t now = time(NULL);
    snprintf(commit_id, sizeof(commit_id), "%ld", now);

    FILE *index = fopen(INDEX_FILE, "r");
    if (!index) return;

    char filename[MAX_PATH_LEN];
    char log_path[MAX_PATH_LEN];
    get_branch_log_path(log_path);

    FILE *log = fopen(log_path, "a");
    if (!log) {
        fclose(index);
        return;
    }

    fprintf(log, "commit %s\nmessage: %s\nfiles:\n", commit_id, message);

    snprintf(log_path, MAX_PATH_LEN, "%s/", OBJECTS_DIR);

    char branch[MAX_PATH_LEN];
    get_current_branch(branch);
    char head_file[MAX_PATH_LEN];
    snprintf(head_file, MAX_PATH_LEN, "%s/%s.txt", BRANCH_HEADS, branch);
    FILE *head = fopen(head_file, "a");

    while (fgets(filename, sizeof(filename), index)) {
        filename[strcspn(filename, "\n")] = 0;
        char hash[HASH_SIZE];
        simple_hash_file(filename, hash);
        write_object(filename, hash);
        fprintf(log, "- %s : %s\n", filename, hash);
        if (head) fprintf(head, "- %s : %s\n", filename, hash);
    }

    fprintf(log, "\n");
    fclose(index);
    fclose(log);
    if (head) fclose(head);

    FILE *commit_file = fopen(COMMIT_FILE, "w");
    if (commit_file) {
        fprintf(commit_file, "%s", commit_id);
        fclose(commit_file);
    }

    index = fopen(INDEX_FILE, "w");
    if (index) fclose(index);

    // Add to commit tree
    CommitNode *new_node = create_commit_node(commit_id, message, commit_tree_root);
    if (commit_tree_root) add_commit_edge(commit_tree_root->id, commit_id);
    commit_tree_root = new_node;

    printf(COLOR_GREEN "Committed as %s\n" COLOR_RESET, commit_id);
}

int is_tracked(const char *filename, char *last_hash_out) {
    char log_path[MAX_PATH_LEN];
    get_branch_log_path(log_path);
    FILE *log = fopen(log_path, "r");
    if (!log) return 0;

    char line[512], last_hash[HASH_SIZE] = "";
    int found = 0;
    while (fgets(line, sizeof(line), log)) {
        if (strncmp(line, "- ", 2) == 0 && strstr(line, filename)) {
            sscanf(line, "- %*s : %40s", last_hash);
            found = 1;
        }
    }
    fclose(log);
    if (found && last_hash_out) strcpy(last_hash_out, last_hash);
    return found;
}

void show_status() {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;

    if ((dir = opendir(".")) == NULL) {
        perror("opendir");
        return;
    }

    printf("Changes in working directory:\n");
    int changes = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        if (stat(entry->d_name, &file_stat) == -1) continue;
        if (S_ISDIR(file_stat.st_mode)) continue;

        if (strcmp(entry->d_name, "vcs") == 0 || strncmp(entry->d_name, ".myvcs", 6) == 0) continue;

        char hash[HASH_SIZE];
        simple_hash_file(entry->d_name, hash);

        char last_hash[HASH_SIZE];
        int tracked = is_tracked(entry->d_name, last_hash);

        if (!tracked) {
            printf(COLOR_YELLOW "  new file: %s\n" COLOR_RESET, entry->d_name);
            changes++;
        } else if (strcmp(hash, last_hash) != 0) {
            printf(COLOR_RED "  modified: %s\n" COLOR_RESET, entry->d_name);
            changes++;
        }
    }
    closedir(dir);

    if (changes == 0) {
        printf("  (no changes detected)\n");
    }
}

void show_log() {
    char log_path[MAX_PATH_LEN];
    get_branch_log_path(log_path);
    FILE *log = fopen(log_path, "r");
    if (!log) return;
    char line[256];
    while (fgets(line, sizeof(line), log)) {
        printf("%s", line);
    }
    fclose(log);
}

void create_branch(const char *branch_name) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s.log", BRANCHES_DIR, branch_name);
    FILE *f = fopen(path, "w");
    if (!f) {
        printf("Failed to create branch.\n");
        return;
    }
    fclose(f);

    // Copy HEAD from current branch
    char current[MAX_PATH_LEN];
    get_current_branch(current);
    char from[MAX_PATH_LEN], to[MAX_PATH_LEN];
    snprintf(from, MAX_PATH_LEN, "%s/%s.txt", BRANCH_HEADS, current);
    snprintf(to, MAX_PATH_LEN, "%s/%s.txt", BRANCH_HEADS, branch_name);
    copy_file(from, to);

    printf("Branch '%s' created.\n", branch_name);
}

void checkout_branch(const char *branch_name) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s.txt", BRANCH_HEADS, branch_name);
    if (access(path, F_OK) != 0) {
        printf("Branch '%s' does not exist.\n", branch_name);
        return;
    }

    // Clean current working directory
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;

    if ((dir = opendir(".")) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (stat(entry->d_name, &file_stat) == -1) continue;
            if (!S_ISDIR(file_stat.st_mode) && 
                strcmp(entry->d_name, "vcs") != 0 && 
                strncmp(entry->d_name, ".myvcs", 6) != 0) {
                remove(entry->d_name);
            }
        }
        closedir(dir);
    }

    FILE *log = fopen(path, "r");
    if (!log) return;

    char line[512], filename[MAX_PATH_LEN], hash[HASH_SIZE];
    while (fgets(line, sizeof(line), log)) {
        if (strncmp(line, "- ", 2) == 0) {
            sscanf(line, "- %s : %s", filename, hash);
            char obj_path[MAX_PATH_LEN];
            snprintf(obj_path, sizeof(obj_path), "%s/%s", OBJECTS_DIR, hash);
            FILE *src = fopen(obj_path, "rb");
            FILE *dest = fopen(filename, "wb");
            if (src && dest) {
                char buf[1024];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
                    fwrite(buf, 1, n, dest);
                }
            }
            if (src) fclose(src);
            if (dest) fclose(dest);
        }
    }
    fclose(log);

    FILE *head = fopen(HEAD_FILE, "w");
    if (head) {
        fprintf(head, "%s", branch_name);
        fclose(head);
    }

    printf("Switched to branch '%s'\n", branch_name);
}

void vcs_revert(const char *commit_id) {
    char branch[MAX_PATH_LEN];
    get_current_branch(branch);

    char log_path[MAX_PATH_LEN];
    snprintf(log_path, sizeof(log_path), "%s/%s.log", BRANCHES_DIR, branch);

    FILE *log = fopen(log_path, "r");
    if (!log) {
        printf("Failed to open branch log.\n");
        return;
    }

    char line[512];
    int found = 0;

    while (fgets(line, sizeof(line), log)) {
        if (strncmp(line, "commit", 6) == 0) {
            char id[64];
            sscanf(line, "commit %s", id);
            if (strncmp(id, commit_id, strlen(commit_id)) == 0) {
                found = 1;
                break;
            }
        }
    }

    if (!found) {
        printf("Commit ID not found.\n");
        fclose(log);
        return;
    }

    fseek(log, 0, SEEK_SET);
    int inside_target = 0;
    FILE *index = fopen(INDEX_FILE, "w");
    if (!index) {
        printf("Failed to open index.\n");
        fclose(log);
        return;
    }

    while (fgets(line, sizeof(line), log)) {
        if (strncmp(line, "commit", 6) == 0) {
            char id[64];
            sscanf(line, "commit %s", id);
            if (strncmp(id, commit_id, strlen(commit_id)) == 0) {
                inside_target = 1;
                continue;
            } else if (inside_target) {
                break;
            }
        }

        if (inside_target && strncmp(line, "- ", 2) == 0) {
            fputs(line, index);
            char filename[MAX_PATH_LEN], hash[HASH_SIZE];
            sscanf(line, "- %s : %s", filename, hash);

            char obj_path[MAX_PATH_LEN];
            snprintf(obj_path, sizeof(obj_path), "%s/%s", OBJECTS_DIR, hash);

            FILE *src = fopen(obj_path, "rb");
            FILE *dest = fopen(filename, "wb");
            if (src && dest) {
                char buf[1024];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
                    fwrite(buf, 1, n, dest);
                }
            }
            if (src) fclose(src);
            if (dest) fclose(dest);
        }
    }

    fclose(index);
    fclose(log);

    system("./vcs commit -m \"Revert commit\"");
}

void vcs_merge(const char *branch_to_merge) {
    char current_branch[MAX_PATH_LEN];
    get_current_branch(current_branch);

    if (strcmp(current_branch, branch_to_merge) == 0) {
        printf("Cannot merge a branch with itself.\n");
        return;
    }

    char current_log_path[MAX_PATH_LEN], merge_log_path[MAX_PATH_LEN];
    snprintf(current_log_path, sizeof(current_log_path), "%s/%s.log", BRANCHES_DIR, current_branch);
    snprintf(merge_log_path, sizeof(merge_log_path), "%s/%s.log", BRANCHES_DIR, branch_to_merge);

    FILE *merge_log = fopen(merge_log_path, "r");
    if (!merge_log) {
        printf("Branch '%s' not found.\n", branch_to_merge);
        return;
    }

    FILE *index = fopen(INDEX_FILE, "a");
    if (!index) {
        printf("Failed to open index for merging.\n");
        fclose(merge_log);
        return;
    }

    char line[512];
    int inside_commit = 0;
    while (fgets(line, sizeof(line), merge_log)) {
        if (strncmp(line, "commit", 6) == 0) {
            inside_commit = 1;
        } else if (inside_commit && strncmp(line, "- ", 2) == 0) {
            char filename[MAX_PATH_LEN], hash[HASH_SIZE];
            sscanf(line, "- %s : %s", filename, hash);

            char obj_path[MAX_PATH_LEN];
            snprintf(obj_path, sizeof(obj_path), "%s/%s", OBJECTS_DIR, hash);
            FILE *src = fopen(obj_path, "rb");
            FILE *dest = fopen(filename, "wb");
            if (src && dest) {
                char buf[1024];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
                    fwrite(buf, 1, n, dest);
                }
            }
            if (src) fclose(src);
            if (dest) fclose(dest);

            fprintf(index, "- %s : %s\n", filename, hash);
        }
    }

    fclose(index);
    fclose(merge_log);

    printf("Merged changes from branch '%s'. Please commit the merge.\n", branch_to_merge);
}

void show_help() {
    printf("Available commands:\n");
    printf("  init              Initialize a new repository\n");
    printf("  add <file>        Add file to staging area\n");
    printf("  commit <msg>      Commit staged files with message\n");
    printf("  status            Show status of working directory\n");
    printf("  log               Show commit history\n");
    printf("  branch <name>     Create a new branch\n");
    printf("  checkout <name>   Switch to the specified branch\n");
    printf("  help              Show this help message\n");
    printf("  revert            To jump to previous version give commit id\n");
    printf("  merge             To merge branches\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: vcs <command> [args]\n");
        return 1;
    }

    if (strcmp(argv[1], "init") == 0) {
        init_repo();
    } else if (strcmp(argv[1], "add") == 0 && argc == 3) {
        add_file(argv[2]);
    } else if (strcmp(argv[1], "commit") == 0 && argc == 3) {
        commit(argv[2]);
    } else if (strcmp(argv[1], "status") == 0) {
        show_status();
    } else if (strcmp(argv[1], "log") == 0) {
        show_log();
    } else if (strcmp(argv[1], "branch") == 0 && argc == 3) {
        create_branch(argv[2]);
    } else if (strcmp(argv[1], "checkout") == 0 && argc == 3) {
        checkout_branch(argv[2]);
    } else if (strcmp(argv[1], "revert") == 0 && argc == 3) {
        vcs_revert(argv[2]);
    } else if (strcmp(argv[1], "help") == 0) {
        show_help();
    } else if (strcmp(argv[1], "merge") == 0 && argc == 3) {
        vcs_merge(argv[2]);
    } else {
        printf("Invalid command. Use 'vcs help' for available commands.\n");
    }
    return 0;
}