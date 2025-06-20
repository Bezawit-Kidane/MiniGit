
class MiniGit : public MiniGitWithBranching {
public:
    // Public interface methods
    void merge(const string& otherBranch); // Merge anotherbranch into current
    void diff(const string& commit1, const string& commit2); // Show differences between commits
    string findLCA(const string& a, const string& b); // find lowest common ancestor commit
// Implementation of merge command - combines changes from another branch
void MiniGit::merge(const string& otherBranch) {
    loadBranches();
    
    // Check if branch exists
    if (!branches.count(otherBranch)) {
        cout << "Branch not found: " << otherBranch << "\n";
        return;
    }

    // Get current branch name
    head = readFile(headFile);
    if (!head.empty()) head.erase(head.find_last_not_of("\n\r\t") + 1);

    // Get the three commits needed for three-way merge:
    string ourCommit = branches[head];      // Current branch's commit
    string theirCommit = branches[otherBranch]; // Other branch's commit
    string baseCommit = findLCA(ourCommit, theirCommit); // Common ancestor

    // Load file states from all three points in history
    auto baseFiles = loadCommitFiles(baseCommit);
    auto ourFiles = loadCommitFiles(ourCommit);
    auto theirFiles = loadCommitFiles(theirCommit);

    // Track all files involved
    unordered_set<string> allFiles;
    for (const auto& pair : baseFiles) allFiles.insert(pair.first);
    for (const auto& pair : ourFiles) allFiles.insert(pair.first);
    for (const auto& pair : theirFiles) allFiles.insert(pair.first);

    bool hasConflicts = false;

    // Process each file for three-way merge
    for (const auto& file : allFiles) {
        // Get file content from all three versions (empty if file didn't exist)
        string baseContent = baseFiles.count(file) ? readFile(objectsDir + "/" + baseFiles[file]) : "";
        string ourContent = ourFiles.count(file) ? readFile(objectsDir + "/" + ourFiles[file]) : "";
        string theirContent = theirFiles.count(file) ? readFile(objectsDir + "/" + theirFiles[file]) : "";

        /* Merge cases:
        1. Both branches made same change -> take either
        2. Only one branch changed -> take that change
        3. Both changed differently -> conflict
        4. File added in one branch -> take added version
        5. File deleted in one branch -> handle accordingly */
        if (ourContent == theirContent) {
            // Case 1: No conflict
            if (!ourContent.empty()) writeFile(file, ourContent);
        } 
        else if (baseContent.empty()) {
            // Case 4: New file in both branches
            if (!ourContent.empty() && !theirContent.empty()) {
                hasConflicts = true;
                cout << "CONFLICT: both added " << file << " with different content\n";
                string merged = "<<<<<<< HEAD (" + head + ")\n" + ourContent 
                              + "\n=======\n" + theirContent 
                              + "\n>>>>>>> " + otherBranch + "\n";
                writeFile(file, merged);
            } 
            else if (!theirContent.empty()) {
                writeFile(file, theirContent); // Take theirs if only they added it
            }
        } 
        else if (ourContent.empty() && baseContent == theirContent) {
            remove(file.c_str()); // Case 5: We deleted, they didn't change
        } 
        else if (theirContent.empty() && baseContent == ourContent) {
            continue; // Case 5: They deleted, we didn't change
        } 
        else if (baseContent == ourContent) {
            writeFile(file, theirContent); // Case 2: We didn't change
        } 
        else if (baseContent == theirContent) {
            writeFile(file, ourContent); // Case 2: They didn't change
        } 
        else {
            // Case 3: Conflict - both changed differently
            hasConflicts = true;
            cout << "CONFLICT: both modified " << file << "\n";
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

// Finalize merge
    if (hasConflicts) {
        cout << "Automatic merge failed; fix conflicts and commit the result.\n";
    } else {
        // Create merge commit with two parents
        commit("Merge branch " + otherBranch, theirCommit);
    }
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
        // Get content from both commits (empty string if file didn't exist)
        string content1 = files1.count(file) ? readFile(objectsDir + "/" + files1[file]) : "";
        string content2 = files2.count(file) ? readFile(objectsDir + "/" + files2[file]) : "";

        // Only show diff if files differ
        if (content1 != content2) {
            // Show file headers with abbreviated commit hashes
            cout << "--- " << file << " (" << commit1.substr(0,7) << ")\n";
            cout << "+++ " << file << " (" << commit2.substr(0,7) << ")\n";
            
            // Line-by-line comparison
            istringstream iss1(content1);
            istringstream iss2(content2);
            string line1, line2;
            int lineNum = 1;
            
            while (getline(iss1, line1) || getline(iss2, line2)) {
                if (line1 != line2) {
                    // Unified diff format
                    cout << "@@ -" << lineNum << " +" << lineNum << " @@\n";
                    if (!line1.empty()) cout << "-" << line1 << "\n"; // Removed line
                    if (!line2.empty()) cout << "+" << line2 << "\n"; // Added line
                }
                lineNum++;
            }
            cout << "\n"; // Separate file diffs
        }
    }
}