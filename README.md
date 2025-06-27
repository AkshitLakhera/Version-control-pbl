# 🔧 Simple Version Control System (VCS) in C

A lightweight custom version control system built in C. It allows you to track changes to files, make commits, view logs, and revert back to previous versions — all from your terminal.

---

## ✅ Features Implemented

- `init` — Initialize a new repository.
- `add <filename>` — Add file to staging (index).
- `commit <message>` — Save snapshot of staged files.
- `log` — View commit history.
- `status` — Check file changes since last commit.
- `checkout <commit_id>` — Revert files to a previous commit state.

---

## 📁 Folder Structure

```bash
Version-control-pbl/
├── src/                 # Source code
│   ├── main.c
│   ├── vcs              # Compiled binary
│   ├── vcs.h
│   ├── Makefile
├── test_files/          # Sample files for testing
├── README.md            # Usage documentation
```

---

## 📦 Setup Instructions

Follow these steps to set up and run the project on your system:

### 1. Clone the Repository

```bash
git clone <your-repo-link>
cd Version-control-pbl
```

### 2. Compile the Project

Navigate to the `src` directory and run `make` to build the project:

```bash
cd src
make
```

This will compile the code and generate the `vcs` executable inside the `src/` directory.

### 3. (Optional) Add `vcs` to Your PATH

To use `vcs` from anywhere in the terminal:

```bash
export PATH=$PATH:$(pwd)
```

You can also add this line to your shell config (`~/.bashrc`, `~/.zshrc`, etc.) for persistence.

---

## 🚀 Ready to Use!

Now you're ready to use your custom VCS. Try commands like:

```bash
vcs init
vcs add <filename>
vcs commit "Initial commit"
vcs log
vcs status
vcs checkout <commit_id>
```

Refer to the [Usage Guide](#🚀-usage-guide) below for detailed command usage.
