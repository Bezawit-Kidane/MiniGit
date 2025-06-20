#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <ctime>
#include <vector>
#include <cstdio>
#include <sys/stat.h> // mkdir for Windows use <direct.h>

// Windows compatibility for directory creation
#ifdef _WIN32
#include <direct.h>
#define mkdir(dir, mode) _mkdir(dir)
#endif

using namespace std; // Using the std namespace

// Simple hash function (djb2) for demonstration (not cryptographically secure)
// Note: This is for demonstration only, not cryptographically secure
unsigned long simpleHash(const string& str) {
    unsigned long hash = 5381; // Initial prime number
    for (size_t i = 0; i < str.size(); ++i) {
        //hash* 33 + current character
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(str[i]);
    }
    return hash;
}

// Convert hash value to hexadecimal string representation
string hashToString(unsigned long h) {
    ostringstream oss;
    oss << hex << h; // Convert to hex format
    return oss.str();
}

// Create directory if not exists
void createDir(const string& dir) {
    struct stat info;
    if (stat(dir.c_str(), &info) != 0) { // Check if directory exixts
        mkdir(dir.c_str(), 0755); // Create with read/write/execute permissions
    }
}

// Read whole file content
string readFile(const string& filename) {
    ifstream ifs(filename.c_str(), ios::binary);
    if (!ifs) return ""; // Return empty string if file cannot be opened
    ostringstream oss;
    oss << ifs.rdbuf(); // Read entire file content
    return oss.str();
}

// Write content to file
void writeFile(const string& filename, const string& content) {
    ofstream ofs(filename.c_str(), ios::binary);
    ofs << content; // Write content to file
}

// MiniGit class
class MiniGit {
    // Repository directory strucutre
    string repoDir = ".minigit";
    string objectsDir = ".minigit/objects"; // Stores all file versions
    string headFile = ".minigit/HEAD"; // Current branch reference
    string indexFile = ".minigit/index"; //Staging area tracking
    string branchesFile = ".minigit/branches"; //Branch pointers storage

    // Data strucutre
    unordered_set<string> stagingArea; // Files staged for next commit
    unordered_map<string, string> branches; // Maos branch names to commit hashes
    string head = "master"; // Current branch (deafult: master)

public:
    // Public interface methods
    void merge(const string& otherBranch); // Merge anotherbranch into current
    void diff(const string& commit1, const string& commit2); // Show differences between commits
    string findLCA(const string& a, const string& b); // find lowest common ancestor commit

    // Initialize a new repository
    void init() {
        createDir(repoDir); // Create main repository directory
        createDir(objectsDir); // Create objects storage

        // Initialize HEAD file if empty
        if (readFile(headFile).empty()) {
            writeFile(headFile, head);
        }

        // Initialize branches file if empty
        if (readFile(branchesFile).empty()) {
            branches[head] = ""; // Master branch with no commits yet
            saveBranches();
        } else {
            loadBranches(); // Load existing branches
        }
        cout << "Initialized empty MiniGit repository.\n";
    }

    // Add file to staging area

    void add(const string& filename) {
        string content = readFile(filename);
        if (content.empty()) {
            cout << "File not found or empty: " << filename << "\n";
            return;
        }
        stagingArea.insert(filename); // Add to staging set
        saveIndex(); // Persist staging area
        cout << "Added " << filename << " to staging area.\n";
    }
    
    // Create a new commit with staged changes
    // Supports regular commits and merge commits (with second parents)
    void commit(const string& message, const string& secondParent = "") {
        loadIndex(); // Load current staging area

        // Check if there are changes to commit
        if (stagingArea.empty()) {
            cout << "No changes staged for commit.\n";
            return;
        }
    
        loadBranches(); // Ensure we have latest branch info
        head = readFile(headFile);
        if (!head.empty()) head.erase(head.find_last_not_of(" \n\r\t")+1); // Trim whitespace

        string parent = ""; // Parent commit (none for first commit)

        // Get parent commit from current branch
        unordered_map<string, string>::const_iterator itb = branches.find(head);
        if (itb != branches.end()) {
            parent = itb->second;
        }

        // Load files from parent commit (if exists)
        unordered_map<string, string> files;
        if (!parent.empty()) {
            files = loadCommitFiles(parent);
        }

        // Add staged files
        unordered_set<string>::const_iterator it;
        for (it = stagingArea.begin(); it != stagingArea.end(); ++it) {
            const string& f = *it;
            string content = readFile(f);
            string blobHash = hashToString(simpleHash(content)); // Generate content hash
            string blobPath = objectsDir + "/" + blobHash;

            // Store file content if not already in objects
            if (readFile(blobPath).empty()) {
                writeFile(blobPath, content);
            }
            files[f] = blobHash; // Update file->hash mapping
        }

        // Create commit content
        ostringstream oss;
        oss << "parent " << parent << "\n"; // Parent commit reference
        if (!secondParent.empty()) {
            oss << "parent2 " << secondParent << "\n"; // Second parent for merge commits
        }
        oss << "date " << time(nullptr) << "\n"; // Current timestamp
        oss << "message " << message << "\n"; // User-provided message
        // Add all file references to commit
        unordered_map<string, string>::const_iterator itf;

        // Add all file references to commit
        for (itf = files.begin(); itf != files.end(); ++itf) {
            oss << "file " << itf->first << " " << itf->second << "\n";
        }
        // Finalize commit object
        string commitContent = oss.str();
        string commitHash = hashToString(simpleHash(commitContent));
        string commitPath = objectsDir + "/" + commitHash;
        writeFile(commitPath, commitContent);

        // Update branch pointer to new commit
        branches[head] = commitHash;
        saveBranches();
        // Clear staging area
        stagingArea.clear();
        saveIndex();

        cout << "Committed to " << head << ": " << commitHash << "\n";
    }
    // Display commit history
    void log() {
        loadBranches();
        head = readFile(headFile);
        if (!head.empty()) head.erase(head.find_last_not_of(" \n\r\t")+1);

        // Get current commit from branch
        string current = "";
        unordered_map<string, string>::const_iterator itb = branches.find(head);
        if (itb != branches.end()) {
            current = itb->second;
        }
        
        // Walk through commit history
        while (!current.empty()) {
            string commitPath = objectsDir + "/" + current;
            string content = readFile(commitPath);
            if (content.empty()) break;
            // Parse commit metadata
            istringstream iss(content);
            string line;
            string parent, parent2, message;
            time_t date = 0;
            while (getline(iss, line)) {
                if (line.find("parent ") == 0) parent = line.substr(7);
                else if (line.find("parent2 ") == 0) parent2 = line.substr(8); 
                else if (line.find("date ") == 0) date = stoll(line.substr(5));
                else if (line.find("message ") == 0) message = line.substr(8);
            } 

            // Display commit info
            cout << "Commit: " << current << "\n";
            if (!parent2.empty()) {
                cout << "Merge: " << parent2.substr(0, 7) << "\n"; // Show merge parent
            }
            cout << "Date: " << ctime(&date); // Covert timestamp to readable format
            cout << "Message: " << message << "\n\n";
            current = parent; // Move to parent commit
        }
    }
    // Create a new branch
    void branch(const string& name) {
        loadBranches();

        // Check if branch already exists
        if (branches.count(name)) {
            cout << "Branch already exists: " << name << "\n";
            return;
        }
        loadBranches();
        head = readFile(headFile);
        if (!head.empty()) head.erase(head.find_last_not_of(" \n\r\t")+1);

        // Get current commit to base new branch on
        unordered_map<string, string>::const_iterator itb = branches.find(head);
        string currentCommit = "";
        if (itb != branches.end()) currentCommit = itb->second;
        branches[name] = currentCommit; // Create new branch pointer
        saveBranches();
        cout << "Created branch " << name << "\n";
    }
    // Switch branches or check out specific commit
    void checkout(const string& name) {
        loadBranches();

        // Check if it's a branch name
        if (branches.count(name)) {
            head = name;
            writeFile(headFile, head);
            restoreCommit(branches[head]); // Restore branch's commit state
            cout << "Switched to branch " << name << "\n";
        } 
        // Otherwise try to treat as commit hash 
        else if (restoreCommit(name)) {
            cout << "Checked out commit " << name << "\n";
        } else {
            cout << "Branch or commit not found: " << name << "\n";
        }
    }

private:
    // Save staging area to index file
    void saveIndex() {
        ofstream ofs(indexFile.c_str());
        unordered_set<string>::const_iterator it;
        for (it = stagingArea.begin(); it != stagingArea.end(); ++it) {
            ofs << *it << "\n"; // Write each staged filename
        }
    }
    
    // Load staging area from index file
    void loadIndex() {
        stagingArea.clear();
        ifstream ifs(indexFile.c_str());
        string line;
        while (getline(ifs, line)) {
            if (!line.empty()) stagingArea.insert(line); // Add each filename
        }
    }
    
    // Save branch information to file
    void saveBranches() {
        ofstream ofs(branchesFile.c_str());
        for (const auto& pair : branches) {
            ofs << pair.first << " " << pair.second << "\n"; // Format: branchname commit_hash
        
        }
    }

    // Load branch information from file
    void loadBranches() {
        branches.clear();
        ifstream ifs(branchesFile.c_str());
        string line;
        while (getline(ifs, line)) {
            if (line.empty()) continue;
            size_t pos = line.find(' ');
            if (pos == string::npos) continue;

            // Parse branchname and commit hash
            string name = line.substr(0, pos);
            string hash = line.substr(pos + 1);
            branches[name] = hash;
        }
    }
    
    //Load file mappings from a commit
    unordered_map<string, string>loadCommitFiles(const string& commitHash) {
        unordered_map<string, string> files;
        string commitPath = objectsDir + "/" + commitHash;
        string content = readFile(commitPath);
        string headFile;
        string objectsDir;
        unordered_map<string, string> branches;

        if (content.empty()) return files;

        // Parse commit content
        istringstream iss(content);
        string line;
        while (getline(iss, line)) {
            if (line.find("file ") == 0) { // File entry line
                size_t pos1 = line.find(' ', 5);
                if (pos1 != string::npos) {
                    // Extract filename and blob hash
                    string fname = line.substr(5, pos1 - 5);
                    string blob = line.substr(pos1 + 1);
                    files[fname] = blob;
                }
            }
        }
        return files;
    }

    // Restore working directory to a specific commit state

    bool restoreCommit(const string& commitHash) {
        if (commitHash.empty()) return false;

        // Get all files from commit
        unordered_map<string, string> files = loadCommitFiles(commitHash);
        unordered_map<string, string>::const_iterator it;

        // Write each file to working directory
        for (it = files.begin(); it != files.end(); ++it) {
            string blobPath = objectsDir + "/" + it->second;
            string content = readFile(blobPath);
            if (!content.empty()) {
                writeFile(it->first, content);
            }
        }
        return true;
    }
};
// Main program entry point

int main(int argc, char* argv[]) {
    MiniGit mg;

    if (argc < 2) {
        cout << "Usage: minigit <command> [args]\n";
        return 1;
    }

    string cmd = argv[1];
    // Command routing
    if (cmd == "init") {
        mg.init();
    } else if (cmd == "add" && argc == 3) {
        mg.add(argv[2]);
    } else if (cmd == "commit" && argc == 4 && string(argv[2]) == "-m") {
        mg.commit(argv[3]);
    } else if (cmd == "log") {
        mg.log();
    } else if (cmd == "branch" && argc == 3) {
        mg.branch(argv[2]);
    } else if (cmd == "checkout" && argc == 3) {
        mg.checkout(argv[2]);
    } else if (cmd == "merge" && argc == 3) {
        mg.merge(argv[2]);
    }
    else if (cmd == "diff" && argc == 4) {
        mg.diff(argv[2], argv[3]);
    }
    else {
        cout << "Unknown or incomplete command.\n";
    }

    return 0;
}
// Implementation of merge command - combines changes from another branch
void MiniGit::merge(const string& otherBranch) {
    loadBranches();
    
    // Check if branch exists
    if (!branches.count(otherBranch)) {
        cout << "Branch not found: " << otherBranch << "\n";
        return;
    }

    head = readFile(headFile);
    if (!head.empty()) head.erase(head.find_last_not_of("\n\r\t") + 1);
    // Get commit hashes for three-way merge
    string ourCommit = branches[head]; // Current branch's commit
    string theirCommit = branches[otherBranch]; // Other branch's commit
    string baseCommit = findLCA(ourCommit, theirCommit); // Common ancestor
    
    // Load file states from all three points
    auto base = loadCommitFiles(baseCommit);
    auto ours = loadCommitFiles(ourCommit);
    auto theirs = loadCommitFiles(theirCommit);
    
    // Collect all files involved
    unordered_set<string> allFiles;
    for (const auto& pair : base) allFiles.insert(pair.first);
    for (const auto& pair : ours) allFiles.insert(pair.first);
    for (const auto& pair : theirs) allFiles.insert(pair.first);

    bool hasConflicts = false; // Track if any conflicts occured
    
    // Three-way  merge for each file
    for (const auto& file : allFiles) {
        // Get file content from all three versions
        string baseContent = base.count(file) ? readFile(objectsDir + "/" + base[file]) : "";
        string ourContent = ours.count(file) ? readFile(objectsDir + "/" + ours[file]) : "";
        string theirContent = theirs.count(file) ? readFile(objectsDir + "/" + theirs[file]) : "";
        
        // Merge cases (similar to git's merge strategy)
        if (ourContent == theirContent) {
            // No conflict - both branches have same content
            if (!ourContent.empty()) writeFile(file, ourContent);
        } 
        else if (baseContent.empty()) {
            // New file in both branches - conflict if both modified
            if (!ourContent.empty() && !theirContent.empty()) {
                hasConflicts = true;
                cout << "CONFLICT: both modified " << file << "\n";
                // Create conflict marker with branch info
                string merged = "<<<<<<< HEAD (" + head + ")\n" + ourContent 
                                + "\n=======\n" + theirContent 
                                + "\n>>>>>>> " + otherBranch + "\n";
                writeFile(file, merged);
            } 
            else if (!theirContent.empty()) {
                // Only in theirs - take their version
                writeFile(file, theirContent);
            }
        } 
        else if (ourContent.empty() && baseContent == theirContent) {
            remove(file.c_str()); // deleted in ours, unchanged in theirs
        } 
        else if (theirContent.empty() && baseContent == ourContent) {
            continue; // deleted in theirs, unchanged in ours
        } 
        else if (baseContent == ourContent) {
            // We didn't change - take theirs
            writeFile(file, theirContent);
        } 
        else if (baseContent == theirContent) {
            // They didn't change - take ours
            writeFile(file, ourContent);
        } 
        else {
            // Both changed differently - conflict
            hasConflicts = true;
            cout << "CONFLICT: both modified " << file << "\n";
            // Create conflict marker with branch info
            string merged = "<<<<<<< HEAD (" + head + ")\n" + ourContent 
                            + "\n=======\n" + theirContent 
                            + "\n>>>>>>> " + otherBranch + "\n";
            writeFile(file, merged);
        }

        // Stage the file if it exists in either branch

        if (!ourContent.empty() || !theirContent.empty()) {
            add(file);
        }
    }
    
    // Handle merge result

    if (hasConflicts) {
        cout << "Automatic merge failed; fix conflicts and commit the result.\n";
    } else {
        // Create merge commit with two parents
        
        commit("Merge branch " + otherBranch, theirCommit);
    }
}

// Find lowest common ancestor of two commits (for mergebase)
string MiniGit::findLCA(const string& a, const string& b) {
    unordered_set<string> ancestors; // Store commit hashes
    
    // Collect all ancestors of commit A
    string current = a;
    while (!current.empty()) {
        ancestors.insert(current);
        string content = readFile(objectsDir + "/" + current);
        istringstream iss(content);
        string line;
        bool foundParent = false;

        // Finnd parent commit reference
        while (getline(iss, line)) {
            if(line.find("parent") == 0) {
                current = line.substr(7);
                foundParent = true;
                break;
            }
        }
        if (!foundParent) break; // No more parents
    }

    // Walk through commit B's ancestor looking for match
    current = b;
    while (!current.empty()) {
        if(ancestors.count(current)) return current; // Found common ancestor
        string content = readFile(objectsDir + "/" + current);
        istringstream iss(content);
        string line;
        bool foundParent = false;
        while (getline(iss, line)) {
            if (line.find("parent") == 0) {
                current = line.substr(7);
                foundParent = true;
                break;
            }
        }
        if(!foundParent) break; // No more parents
    }
    return ""; // No common ancestor found
    
}

// Show differences between two commits

void MiniGit::diff(const string& commit1, const string& commit2) {
    // Load files from both commits
    auto files1 = loadCommitFiles(commit1);
    auto files2 = loadCommitFiles(commit2);
    
    // Collect all unique files from both commits
    unordered_set<string> allFiles;
    for (const auto& pair : files1) allFiles.insert(pair.first);
    for (const auto& pair : files2) allFiles.insert(pair.first);
    // Compare each file
    for (const auto& file : allFiles) {
        // Get file content from both commits (empty if file exist)
        string content1 = files1.count(file) ? readFile(objectsDir + "/" + files1[file]) : "";
        string content2 = files2.count(file) ? readFile(objectsDir + "/" + files2[file]) : "";
        
        // Only show diff if files are different
        if (content1 != content2) {
            // Show file headers with abbreviated commit hashes
            cout << "--- " << file << " (" << commit1.substr(0,7) << ")\n";
            cout << "+++ " << file << " (" << commit2.substr(0,7) << ")\n";
            
            // Line-by-line comparsion
            istringstream iss1(content1);
            istringstream iss2(content2);
            string line1, line2;
            int lineNum = 1;
            
            while (getline(iss1, line1) || getline(iss2, line2)) {
                if (line1 != line2) {
                    // Show line number and changes
                    cout << "@@ -" << lineNum << " +" << lineNum << " @@\n";
                    if (!line1.empty()) cout << "-" << line1 << "\n"; // Removed line
                    if (!line2.empty()) cout << "+" << line2 << "\n"; // Added line
                }
                lineNum++;
            }
            cout << "\n"; // Separate diffs with blank line
        }
    }
}