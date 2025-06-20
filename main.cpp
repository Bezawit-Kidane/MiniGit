#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <ctime>
#include <vector>
#include <cstdio>
#include <sys/stat.h> // mkdir for windows use <direct.h>

// Windows compatibility for directory creation
#ifdef _WIN32
#include <direct.h>
#define mkdir(dir, mode) _mkdir(dir)
#endif

using namespace std; // Using the std namespace


// Simple hash function (djb2) for demonstration (not cryptographically secure)
// Note: This is for demonstration only, not cryptographically secure
unsigned long simpleHash(const string& str){
    unsigned long hash = 5381; // Initial prime number
    for(size_t i=0; i < str.size(); ++i) {
        //hash* 33 + current character
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(str[i]);
    }

    return hash;
}

// Convert hash value to hexadecimal string representation
string hashToString(unsigned long h) {
    ostringstream oss;
    oss <<hex <<h; // Convert to hex format
    return oss.str();
}

// Create directory if not exists
void createDir(const string& dir) {
    struct stat info;
    if (stat(dir.c_str(), &info) !=0) { // Check if directory exixts
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
    string repoDir = ".mingit";
    string objectsDir = ".minigit/objects"; // Stores all file versions
    string headFile = ".minigit/HEAD";// Current branch reference
    string indexFile = ".minigit/index"; //Staging area tracking
    string branchesfile = ".minigit/branches"; //Branch pointers storage

    // Data structure
    unordered_set<string> stagingArea; // Files staged for next commit
    unordered_map<string, string> branches;  // Maos branch names to commit hashes
    string head = "master"; // Current branch (deafult: master)


public:
  // Initialize a new repository
  void init() {
    if(ifstream(headFile)){
     cout<< "MiniGit repository already exists.\n";
      return;

    }
    createDir(repoDir); // Create main repository directory
    createDir(objectsDir); // Create objects storage

    // Initialize HEAD file if empty
    if (readFile(headFile).empty()) {
        writeFile(headFile, head);
    }

    // Initialize branches file if empty
    if (readFile(branchesfile).empty()) {
        branches[head] =""; // Master branch with no commits yet
        saveBranches();
    }
    else {
        loadBranches(); // Load existing branches
    }
    cout << "Intialized empty Minigit repository.\n";
    }

     // Add file to staging area
    void add(const string& filename) {
        string content = readFile(filename);
        if (content.empty()) {
            cout << "File not found or empty: " << filename << "\n";
            return;
        }
        stagingArea.insert (filename); // Add to staging set
        saveIndex(); // Persist staging area
        cout << "Added" << filename << " to staging area.\n";

    }

    // Create a new commit with staged changes
    // Supports regular commits and merge commits (with second parents)
    void commit (const string& messages, const string& secondparent = "") {
        loadIndex(); // Load current staging area

         // Check if there are changes to commit
        if(stagingArea.empty()) {
            cout << "No changes staged for commit.\n";
            return;
        }

        loadBranches(); // Ensure we have latest branch info
        head = readFile(headFile);
        if (!head.empty()) gead.erase(head.find_last_not_of(" \n\r\t")+1);  // Trim whitespace
        string parent = ""; // Parent commit (none for first commit)

        // Get parent commit from current branch
        unordered_map<string, string>:: const_iterator itb = branches.find(head);
        if (itb != branches. end()) {
            parent = itb->second;
        }

        // Load files from parent commit (if exists)
        unordered_map<string, string> files;
        if (!parent.empty()) {
            files = loadCommitFiles(parent);
        }

        //Add staged files
        unordered_set<string>::const_iterator it;
        for(it= stagingArea.begin(); it != stagingArea.end(); ++it) {
            const string& f= *it;
            string content = readFile(f);
            string blobHash = hashToString(simpleHash(content)); // Generate content hash
            string blobPath = objectsDir + "/" + blobHash;
            
             // Store file content if not already in objects
            if (readFile(blobPath). empty()) {
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
}