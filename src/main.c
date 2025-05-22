#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir _mkdir
#else
#include <sys/types.h>
#endif

#define VCS_DIR ".myvcs"
#define OBJECTS_DIR ".myvcs/objects"
#define INDEX_FILE ".myvcs/index"
#define LOG_FILE ".myvcs/log"
#define HEAD_FILE ".myvcs/HEAD"
#define MAX_FILES 100
#define HASH_SIZE 41
#define MAX_COMMIT_MSG 256

#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"

typedef struct CommitNode {
    char id[64];
    char message[MAX_COMMIT_MSG];
    time_t timestamp;
    struct CommitNode *prev;
    struct CommitNode *next;
} CommitNode;

CommitNode *head = NULL;
CommitNode *tail = NULL;

typedef struct {
    char filename[256];
    char hash[HASH_SIZE];
} FileEntry;

FileEntry file_version_map[MAX_FILES];
int file_count = 0;

void simple_hash_file(const char *filename, char *output) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        strcpy(output, "0000000000000000000000000000000000000000");
        return;
    }
    unsigned long hash = 5381;
    int c;
    while ((c = fgetc(file)) != EOF)
        hash = ((hash << 5) + hash) + c;
    fclose(file);
    sprintf(output, "%040lx", hash);
}

void write_object(const char *filename, const char *hash) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", OBJECTS_DIR, hash);
    if (access(path, F_OK) == 0) return;

    FILE *src = fopen(filename, "rb");
    FILE *dest = fopen(path, "wb");
    if (!src || !dest) return;

    char buffer[1024];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0)
        fwrite(buffer, 1, n, dest);

    fclose(src);
    fclose(dest);
}

void load_index(char files[][256], int *count) {
    *count = 0;
    FILE *index = fopen(INDEX_FILE, "r");
    if (!index) return;

    while (fgets(files[*count], 256, index)) {
        files[*count][strcspn(files[*count], "\n")] = 0;
        (*count)++;
    }
    fclose(index);
}

int is_file_in_index(const char *filename) {
    FILE *index = fopen(INDEX_FILE, "r");
    if (!index) return 0;
    char line[256];
    while (fgets(line, sizeof(line), index)) {
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, filename) == 0) {
            fclose(index);
            return 1;
        }
    }
    fclose(index);
    return 0;
}

void show_diff(const char *file1, const char *file2) {
    FILE *f1 = fopen(file1, "r");
    FILE *f2 = fopen(file2, "r");
    if (!f1 || !f2) return;

    char line1[512], line2[512];
    int lineno = 1;
    while (fgets(line1, sizeof(line1), f1) && fgets(line2, sizeof(line2), f2)) {
        if (strcmp(line1, line2) != 0) {
            printf(COLOR_YELLOW "Line %d changed:\n" COLOR_RESET, lineno);
            printf(COLOR_RED "- %s" COLOR_RESET, line1);
            printf(COLOR_GREEN "+ %s" COLOR_RESET, line2);
        }
        lineno++;
    }
    while (fgets(line1, sizeof(line1), f1)) {
        printf(COLOR_YELLOW "Line %d removed:\n" COLOR_RESET, lineno++);
        printf(COLOR_RED "- %s" COLOR_RESET, line1);
    }
    while (fgets(line2, sizeof(line2), f2)) {
        printf(COLOR_YELLOW "Line %d added:\n" COLOR_RESET, lineno++);
        printf(COLOR_GREEN "+ %s" COLOR_RESET, line2);
    }
    fclose(f1);
    fclose(f2);
}

void init_repo() {
#ifdef _WIN32
    _mkdir(VCS_DIR);
    _mkdir(OBJECTS_DIR);
#else
    mkdir(VCS_DIR, 0700);
    mkdir(OBJECTS_DIR, 0700);
#endif
    FILE *f;
    f = fopen(INDEX_FILE, "w"); if (f) fclose(f);
    f = fopen(LOG_FILE, "w"); if (f) fclose(f);
    f = fopen(HEAD_FILE, "w"); if (f) fclose(f);
    printf(COLOR_GREEN "Repository initialized.\n" COLOR_RESET);
}

void add_file(const char *filename) {
    if (is_file_in_index(filename)) {
        printf(COLOR_YELLOW "%s is already added.\n" COLOR_RESET, filename);
        return;
    }
    FILE *index = fopen(INDEX_FILE, "a");
    if (!index) return;
    fprintf(index, "%s\n", filename);
    fclose(index);
    printf(COLOR_GREEN "Added %s\n" COLOR_RESET, filename);
}

void commit(const char *message) {
    char commit_id[64];
    time_t now = time(NULL);
    snprintf(commit_id, sizeof(commit_id), "%ld", now);

    char files[MAX_FILES][256];
    int count;
    load_index(files, &count);

    FILE *log = fopen(LOG_FILE, "a");
    fprintf(log, "commit %s\nmessage: %s\nfiles:\n", commit_id, message);

    for (int i = 0; i < count; i++) {
        char hash[HASH_SIZE];
        simple_hash_file(files[i], hash);
        write_object(files[i], hash);

        int found = 0;
        for (int j = 0; j < file_count; j++) {
            if (strcmp(file_version_map[j].filename, files[i]) == 0) {
                strcpy(file_version_map[j].hash, hash);
                found = 1;
                break;
            }
        }
        if (!found && file_count < MAX_FILES) {
            strcpy(file_version_map[file_count].filename, files[i]);
            strcpy(file_version_map[file_count].hash, hash);
            file_count++;
        }
        fprintf(log, "- %s : %s\n", files[i], hash);
    }
    fprintf(log, "\n");
    fclose(log);

    FILE *head_file = fopen(HEAD_FILE, "w");
    fprintf(head_file, "%s", commit_id);
    fclose(head_file);

    FILE *index = fopen(INDEX_FILE, "w");
    if (index) fclose(index);

    CommitNode *new_node = malloc(sizeof(CommitNode));
    if (!new_node) {
        fprintf(stderr, COLOR_RED "Memory allocation failed.\n" COLOR_RESET);
        return;
    }

    strcpy(new_node->id, commit_id);
    strcpy(new_node->message, message);
    new_node->timestamp = now;
    new_node->prev = tail;
    new_node->next = NULL;

    if (tail) tail->next = new_node;
    tail = new_node;
    if (!head) head = new_node;

    printf(COLOR_CYAN "Committed as %s\n" COLOR_RESET, commit_id);
}

void load_commits() {
    FILE *log = fopen(LOG_FILE, "r");
    if (!log) return;

    char line[512];
    CommitNode *last = NULL;

    while (fgets(line, sizeof(line), log)) {
        if (strncmp(line, "commit", 6) == 0) {
            CommitNode *node = malloc(sizeof(CommitNode));
            sscanf(line, "commit %s", node->id);
            node->message[0] = '\0';
            node->timestamp = atol(node->id);
            node->prev = last;
            node->next = NULL;
            if (last) last->next = node;
            else head = node;
            tail = node;
            last = node;
        } else if (strncmp(line, "message:", 8) == 0 && tail) {
            sscanf(line, "message: %[^\n]", tail->message);
        }
    }
    fclose(log);
}

void show_log() {
    if (!head) {
        printf(COLOR_YELLOW "No commits yet.\n" COLOR_RESET);
        return;
    }
    CommitNode *curr = head;
    while (curr) {
        char time_str[64];
        struct tm *tm_info = localtime(&curr->timestamp);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        printf(COLOR_GREEN "Commit %s\n" COLOR_RESET, curr->id);
        printf("Date: %s\n", time_str);
        printf("Message: %s\n\n", curr->message);
        curr = curr->next;
    }
}

void show_status() {
    char files[MAX_FILES][256];
    int count;
    load_index(files, &count);

    if (count == 0) {
        printf(COLOR_YELLOW "No changes to be committed.\n" COLOR_RESET);
        return;
    }

    printf(COLOR_CYAN "Changes to be committed:\n" COLOR_RESET);
    for (int i = 0; i < count; i++) {
        char hash[HASH_SIZE];
        simple_hash_file(files[i], hash);
        printf("- %s : %s\n", files[i], hash);
        for (int j = 0; j < file_count; j++) {
            if (strcmp(files[i], file_version_map[j].filename) == 0) {
                char obj_path[512];
                snprintf(obj_path, sizeof(obj_path), "%s/%s", OBJECTS_DIR, file_version_map[j].hash);
                printf("Diff for %s:\n", files[i]);
                show_diff(obj_path, files[i]);
                break;
            }
        }
    }
}

void checkout(const char *commit_id) {
    FILE *log = fopen(LOG_FILE, "r");
    if (!log) {
        printf(COLOR_RED "No commits found.\n" COLOR_RESET);
        return;
    }

    char line[512];
    int found = 0;
    while (fgets(line, sizeof(line), log)) {
        if (strncmp(line, "commit", 6) == 0 && strstr(line, commit_id)) {
            found = 1;
            continue;
        }
        if (found && strncmp(line, "-", 1) == 0) {
            char file[256], hash[HASH_SIZE];
            sscanf(line, "- %s : %s", file, hash);
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", OBJECTS_DIR, hash);

            FILE *src = fopen(path, "rb");
            FILE *dest = fopen(file, "wb");
            if (!src || !dest) continue;

            char buffer[1024];
            size_t n;
            while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0)
                fwrite(buffer, 1, n, dest);

            fclose(src);
            fclose(dest);
        } else if (found && line[0] == '\n') break;
    }
    fclose(log);
    if (found)
        printf(COLOR_GREEN "Checked out commit %s\n" COLOR_RESET, commit_id);
    else
        printf(COLOR_RED "Commit ID %s not found.\n" COLOR_RESET, commit_id);
}

int main(int argc, char *argv[]) {
    load_commits();  // 🆕 Load commits on every run

    if (argc < 2) {
        printf(COLOR_YELLOW "Usage: vcs <command> [args]\n" COLOR_RESET);
        return 1;
    }

    if (strcmp(argv[1], "init") == 0) {
        init_repo();
    } else if (strcmp(argv[1], "add") == 0 && argc == 3) {
        add_file(argv[2]);
    } else if (strcmp(argv[1], "commit") == 0 && argc == 3) {
        commit(argv[2]);
    } else if (strcmp(argv[1], "log") == 0) {
        show_log();
    } else if (strcmp(argv[1], "status") == 0) {
        show_status();
    } else if (strcmp(argv[1], "checkout") == 0 && argc == 3) {
        checkout(argv[2]);
    } else {
        printf(COLOR_RED "Invalid command.\n" COLOR_RESET);
    }

    return 0;
}
