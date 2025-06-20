
class MiniGit : public MiniGitBase {
public:
    void branch(const std::string& name) {
        if (branches.count(name)) {
            std::cout << "Branch already exists\n";
            return;
        }
        branches[name] = branches[currentBranch];
        saveBranches();
        std::cout << "Created branch " << name << "\n";
    }

    void checkout(const std::string& name) {
        if (!branches.count(name)) {
            std::cout << "Branch doesn't exist\n";
            return;
        }
        currentBranch = name;
        writeFile(headFile, currentBranch);

        std::string commitHash = branches[name];
        auto files = loadCommitFiles(commitHash);
        for (const auto& [file, hash] : files) {
            writeFile(file, readFile(objectsDir + "/" + hash));
        }

        std::cout << "Switched to branch " << name << "\n";
    }
};

int main(int argc, char* argv[]) {
    if (cmd == "branch" && argc == 3) {
        mg.branch(argv[2]);
    } 
    else if (cmd == "checkout" && argc == 3) {
        mg.checkout(argv[2]);
    }
}
