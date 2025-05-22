/*
 * Full Version Control System in C
 * Cross-platform (Linux/macOS/Windows)
 * Features:
 * - Init, Add, Commit, Log, Status, Checkout
 * - Doubly linked list for commit history
 * - Hash map for file-version mapping
 * - Parent-pointer commit graph
 * - Basic line-level diffing
 */

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

// ANSI color codes for output
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"

// --------------------- Data Structures ----------------------

typedef struct CommitNode {
    char id[64];
    char message[MAX_COMMIT_MSG];
    time_t timestamp;
    struct CommitNode *prev;
    struct CommitNode *next;
} CommitNode;

CommitNode *head = NULL;
CommitNode *tail = NULL;

// HashMap (simplified for up to MAX_FILES)
typedef struct {
    char filename[256];
    char hash[HASH_SIZE];
} FileEntry;

FileEntry file_version_map[MAX_FILES];
int file_count = 0;

// --------------------- Utilities ----------------------

int repo_exists() {
    struct stat st;
    return (stat(VCS_DIR, &st) == 0 && S_ISDIR(st.st_mode));
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

int file_exists(const char *filename) {
    return access(filename, F_OK) == 0;
}

void write_object(const char *filename, const char *hash) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", OBJECTS_DIR, hash);

    if (access(path, F_OK) == 0) return;

    FILE *src = fopen(filename, "rb");
    if (!src) {
        printf(COLOR_RED "Error: Cannot read file %s\n" COLOR_RESET, filename);
        return;
    }
    
    FILE *dest = fopen(path, "wb");
    if (!dest) {
        printf(COLOR_RED "Error: Cannot create object file\n" COLOR_RESET);
        fclose(src);
        return;
    }

    char buffer[1024];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, n, dest);
    }
    fclose(src);
    fclose(dest);
}

void load_index(char files[][256], int *count) {
    *count = 0;
    FILE *index = fopen(INDEX_FILE, "r");
    if (!index) return;

    while (fgets(files[*count], 256, index) && *count < MAX_FILES) {
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

void load_file_version_map() {
    file_count = 0;
    FILE *log = fopen(LOG_FILE, "r");
    if (!log) return;

    char line[512];
    while (fgets(line, sizeof(line), log)) {
        if (strncmp(line, "- ", 2) == 0) {
            char filename[256], hash[HASH_SIZE];
            if (sscanf(line, "- %255s : %40s", filename, hash) == 2) {
                // Update or add to hashmap
                int found = 0;
                for (int i = 0; i < file_count; i++) {
                    if (strcmp(file_version_map[i].filename, filename) == 0) {
                        strcpy(file_version_map[i].hash, hash);
                        found = 1;
                        break;
                    }
                }
                if (!found && file_count < MAX_FILES) {
                    strcpy(file_version_map[file_count].filename, filename);
                    strcpy(file_version_map[file_count].hash, hash);
                    file_count++;
                }
            }
        }
    }
    fclose(log);
}

void load_commit_history() {
    // Free existing list
    CommitNode *curr = head;
    while (curr) {
        CommitNode *next = curr->next;
        free(curr);
        curr = next;
    }
    head = tail = NULL;

    FILE *log = fopen(LOG_FILE, "r");
    if (!log) return;

    char line[512];
    while (fgets(line, sizeof(line), log)) {
        if (strncmp(line, "commit ", 7) == 0) {
            CommitNode *new_node = malloc(sizeof(CommitNode));
            if (!new_node) continue;

            // Extract commit ID
            sscanf(line, "commit %63s", new_node->id);
            new_node->timestamp = atol(new_node->id); // Simple conversion

            // Read message line
            if (fgets(line, sizeof(line), log) && strncmp(line, "message: ", 9) == 0) {
                strncpy(new_node->message, line + 9, MAX_COMMIT_MSG - 1);
                new_node->message[MAX_COMMIT_MSG - 1] = '\0';
                new_node->message[strcspn(new_node->message, "\n")] = 0;
            } else {
                strcpy(new_node->message, "No message");
            }

            // Add to linked list
            new_node->prev = tail;
            new_node->next = NULL;
            if (tail) tail->next = new_node;
            tail = new_node;
            if (!head) head = new_node;

            // Skip files section
            while (fgets(line, sizeof(line), log) && line[0] != '\n') {
                // Skip file entries
            }
        }
    }
    fclose(log);
}

// ------------------- Diff ---------------------

void show_diff(const char *file1, const char *file2) {
    FILE *f1 = fopen(file1, "r");
    FILE *f2 = fopen(file2, "r");
    if (!f1 || !f2) {
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        return;
    }

    char line1[512], line2[512];
    int lineno = 1;
    int has_line1, has_line2;
    
    while (1) {
        has_line1 = (fgets(line1, sizeof(line1), f1) != NULL);
        has_line2 = (fgets(line2, sizeof(line2), f2) != NULL);
        
        if (!has_line1 && !has_line2) break;
        
        if (has_line1 && has_line2) {
            if (strcmp(line1, line2) != 0) {
                printf(COLOR_YELLOW "Line %d changed:\n" COLOR_RESET, lineno);
                printf(COLOR_RED "- %s" COLOR_RESET, line1);
                printf(COLOR_GREEN "+ %s" COLOR_RESET, line2);
            }
        } else if (has_line1) {
            printf(COLOR_YELLOW "Line %d removed:\n" COLOR_RESET, lineno);
            printf(COLOR_RED "- %s" COLOR_RESET, line1);
        } else if (has_line2) {
            printf(COLOR_YELLOW "Line %d added:\n" COLOR_RESET, lineno);
            printf(COLOR_GREEN "+ %s" COLOR_RESET, line2);
        }
        lineno++;
    }

    fclose(f1);
    fclose(f2);
}

// ------------------- VCS Commands ---------------------

void init_repo() {
    if (repo_exists()) {
        printf(COLOR_YELLOW "Repository already exists.\n" COLOR_RESET);
        return;
    }

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
    if (!repo_exists()) {
        printf(COLOR_RED "Not a repository. Run 'vcs init' first.\n" COLOR_RESET);
        return;
    }

    if (!file_exists(filename)) {
        printf(COLOR_RED "File '%s' does not exist.\n" COLOR_RESET, filename);
        return;
    }

    if (is_file_in_index(filename)) {
        printf(COLOR_YELLOW "%s is already added.\n" COLOR_RESET, filename);
        return;
    }

    FILE *index = fopen(INDEX_FILE, "a");
    if (!index) {
        printf(COLOR_RED "Error: Cannot open index file.\n" COLOR_RESET);
        return;
    }
    fprintf(index, "%s\n", filename);
    fclose(index);
    printf(COLOR_GREEN "Added %s\n" COLOR_RESET, filename);
}

void commit(const char *message) {
    if (!repo_exists()) {
        printf(COLOR_RED "Not a repository. Run 'vcs init' first.\n" COLOR_RESET);
        return;
    }

    char files[MAX_FILES][256];
    int count;
    load_index(files, &count);

    if (count == 0) {
        printf(COLOR_YELLOW "No changes to commit. Use 'vcs add <file>' first.\n" COLOR_RESET);
        return;
    }

    char commit_id[64];
    time_t now = time(NULL);
    snprintf(commit_id, sizeof(commit_id), "%ld", now);

    // Load existing file version map
    load_file_version_map();

    FILE *log = fopen(LOG_FILE, "a");
    if (!log) {
        printf(COLOR_RED "Error: Cannot open log file.\n" COLOR_RESET);
        return;
    }
    
    fprintf(log, "commit %s\nmessage: %s\nfiles:\n", commit_id, message);

    for (int i = 0; i < count; i++) {
        char hash[HASH_SIZE];
        simple_hash_file(files[i], hash);
        write_object(files[i], hash);

        // Update hashmap (file_version_map)
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
    if (head_file) {
        fprintf(head_file, "%s", commit_id);
        fclose(head_file);
    }

    // Clear index after commit
    FILE *index = fopen(INDEX_FILE, "w");
    if (index) fclose(index);

    // Reload commit history to include this new commit
    load_commit_history();

    printf(COLOR_CYAN "Committed as %s\n" COLOR_RESET, commit_id);
}

void show_log() {
    if (!repo_exists()) {
        printf(COLOR_RED "Not a repository. Run 'vcs init' first.\n" COLOR_RESET);
        return;
    }

    // Load commit history from log file
    load_commit_history();

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
    if (!repo_exists()) {
        printf(COLOR_RED "Not a repository. Run 'vcs init' first.\n" COLOR_RESET);
        return;
    }

    char files[MAX_FILES][256];
    int count;
    load_index(files, &count);

    if (count == 0) {
        printf(COLOR_YELLOW "No changes to be committed.\n" COLOR_RESET);
        return;
    }

    // Load file version map
    load_file_version_map();

    printf(COLOR_CYAN "Changes to be committed:\n" COLOR_RESET);
    for (int i = 0; i < count; i++) {
        char hash[HASH_SIZE];
        simple_hash_file(files[i], hash);
        printf("- %s : %s\n", files[i], hash);

        // Show diff with last committed version
        for (int j = 0; j < file_count; j++) {
            if (strcmp(files[i], file_version_map[j].filename) == 0) {
                char obj_path[512];
                snprintf(obj_path, sizeof(obj_path), "%s/%s", OBJECTS_DIR, file_version_map[j].hash);
                if (file_exists(obj_path)) {
                    printf("Diff for %s:\n", files[i]);
                    show_diff(obj_path, files[i]);
                }
                break;
            }
        }
    }
}

void checkout(const char *commit_id) {
    if (!repo_exists()) {
        printf(COLOR_RED "Not a repository. Run 'vcs init' first.\n" COLOR_RESET);
        return;
    }

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
            // Skip message line
            fgets(line, sizeof(line), log);
            // Skip "files:" line
            fgets(line, sizeof(line), log);
            continue;
        }
        if (found && strncmp(line, "- ", 2) == 0) {
            char file[256], hash[HASH_SIZE];
            if (sscanf(line, "- %255s : %40s", file, hash) == 2) {
                char path[512];
                snprintf(path, sizeof(path), "%s/%s", OBJECTS_DIR, hash);

                if (!file_exists(path)) {
                    printf(COLOR_RED "Object file %s not found.\n" COLOR_RESET, hash);
                    continue;
                }

                FILE *src = fopen(path, "rb");
                FILE *dest = fopen(file, "wb");
                if (!src || !dest) {
                    if (src) fclose(src);
                    if (dest) fclose(dest);
                    continue;
                }

                char buffer[1024];
                size_t n;
                while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                    fwrite(buffer, 1, n, dest);
                }
                fclose(src);
                fclose(dest);
                printf(COLOR_GREEN "Restored %s\n" COLOR_RESET, file);
            }
        } else if (found && line[0] == '\n') {
            break;
        }
    }
    fclose(log);
    
    if (found) {
        // Update HEAD
        FILE *head_file = fopen(HEAD_FILE, "w");
        if (head_file) {
            fprintf(head_file, "%s", commit_id);
            fclose(head_file);
        }
        printf(COLOR_GREEN "Checked out commit %s\n" COLOR_RESET, commit_id);
    } else {
        printf(COLOR_RED "Commit ID %s not found.\n" COLOR_RESET, commit_id);
    }
}

// ---------------------- Main ---------------------------

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf(COLOR_YELLOW "Usage: vcs <command> [args]\n");
        printf("Commands:\n");
        printf("  init                 - Initialize repository\n");
        printf("  add <file>          - Add file to staging\n");
        printf("  commit <message>    - Commit changes\n");
        printf("  log                 - Show commit history\n");
        printf("  status              - Show current status\n");
        printf("  checkout <commit>   - Checkout a commit\n" COLOR_RESET);
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
        printf(COLOR_RED "Invalid command or missing arguments.\n" COLOR_RESET);
        printf(COLOR_YELLOW "Use 'vcs' without arguments to see usage.\n" COLOR_RESET);
    }
    
    return 0;
}