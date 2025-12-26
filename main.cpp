// main.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <array>
#include <memory>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

std::string usageStr =
    "Usage: polyglot <source1> <source2> -o <outputFile>\n"
    "Supported extensions:\n"
    "  C/C++: .cpp, .cc, .cxx, .c\n"
    "  Python: .py\n"
    "  Ruby: .rb\n"
    "  Bash: .sh\n"
    "  Perl: .pl\n";
    
std::string runCmd(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        result += buffer.data();
    return result;
}

static std::string shellSafePath(const std::string& path) {
    std::string s = fs::path(path).generic_string();
    std::string escaped;
    for (char c : s) {
        if (c == '"') escaped += "\\\"";
        else escaped += c;
    }
    return "\"" + escaped + "\"";
}

std::string replace(const std::string& str, const std::string& replace, const std::string& with) {
    if (replace.empty()) return str;
    std::string result;
    result.reserve(str.size());
    std::size_t start = 0;
    std::size_t pos;
    while ((pos = str.find(replace, start)) != std::string::npos) {
        result.append(str, start, pos - start);
        result += with;
        start = pos + replace.length();
    }
    result.append(str, start, str.size() - start);
    return result;
}

bool checkSyntax(const std::string& file, const std::string& ext) {
    std::string res;
    std::string quoted = shellSafePath(file);

    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c") {
        std::string flag = ext == ".c" ? "-x c " : "";
        res = runCmd("g++ -fsyntax-only " + flag + quoted + " 2>&1");
        if (!res.empty()) {
            std::cerr << "C/C++ syntax errors in " << file << ":\n" << res;
            return false;
        }
        return true;
    } else if (ext == ".py") {
        try {
            res = runCmd("python3 -m pyflakes " + quoted + " 2>&1");
        } catch (const std::exception& x) {
            std::cerr << "\033[31m" << "Error: " << x.what() << "\033[0m" << std::endl;
            res = x.what();
        }
        if (!res.empty()) {
            std::string fallback = runCmd("python -m py_compile " + quoted + " 2>&1");
            if (!fallback.empty()) {
                std::cerr << "Python syntax errors in " << file << ":\n" << fallback;
                std::cerr << "If pyflakes is desired, please install it or ensure it's on PATH.\n";
                return false;
            }
        }
        return true;
    } else if (ext == ".rb") {
        res = runCmd("ruby -c " + quoted + " 2>&1");
        if (res.find("Syntax OK") == std::string::npos) {
            std::cerr << "Ruby syntax errors in " << file << ":\n" << res;
            return false;
        }
        return true;
    } else if (ext == ".sh") {
        res = runCmd("bash -n " + quoted + " 2>&1");
        if (!res.empty()) {
            std::cerr << "Bash syntax errors in " << file << ":\n" << res;
            return false;
        }
        return true;
    } else if (ext == ".pl") {
        res = runCmd("perl -c " + quoted + " 2>&1");
        if (res.find("syntax OK") == std::string::npos) {
            std::cerr << "Perl syntax errors in " << file << ":\n" << res;
            return false;
        }
        return true;
    } else if (ext == ".vn") {
        return true; // unsupported
    }

    std::cerr << "\nUnsupported file extension: " << ext << "\n";
    return false;
}

std::vector<std::string> readFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in.is_open()) throw std::runtime_error("Failed to open: " + filename);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    return lines;
}

void writeMerged(
    const std::string& outFile,
    const std::string& ext1, const std::vector<std::string>& content1,
    const std::string& ext2, const std::vector<std::string>& content2
) {
    std::ofstream out(outFile, std::ios::binary);
    if (!out.is_open()) throw std::runtime_error("Failed to open output: " + outFile);

    auto openFence = [](const std::string& ext) -> std::string {
        if (ext == ".py") return "r'''";
        if (ext == ".rb") return "=begin";
        if (ext == ".sh") return ": '";
        if (ext == ".pl") return "=pod";
        if (ext == ".vn") return "!#";
        return "";
    };

    auto closeFence = [](const std::string& ext) -> std::string {
        if (ext == ".py") return "'''";
        if (ext == ".rb") return "=end";
        if (ext == ".sh") return "'";
        if (ext == ".pl") return "=cut";
        if (ext == ".vn") return "#!";
        return "";
    };

    auto escapeCpp = [](const std::string& content) -> std::string {
        return "#if 0\n" + content + "\n#endif\n";
    };

    auto writeLine = [&out](const std::string& line) {
        out.write(line.c_str(), line.size());
        out.put('\n');
    };

    auto escapeForPython = [](const std::string& line) -> std::string {
        std::string out;
        for (size_t i = 0; i < line.size(); i++) {
            if (i + 2 < line.size() && line[i] == '\'' && line[i+1] == '\'' && line[i+2] == '\'') {
                out += "\\'\\'\\'";
                i += 2;
            } else {
                out += line[i];
            }
        }
        return out;
    };

    if (ext1 == ".cpp" || ext1 == ".cc" || ext1 == ".cxx" || ext1 == ".c") {
        writeLine(escapeCpp(openFence(ext2)));
        for (const std::string& l : content1) {
            writeLine(escapeForPython(l));
        }
        writeLine(escapeCpp(closeFence(ext2)));

        writeLine("#if 0");
        for (auto& l : content2) {
            writeLine(escapeForPython(l));
        }
        writeLine("#endif");
    }
    else if (ext2 == ".cpp" || ext2 == ".cc" || ext2 == ".cxx" || ext2 == ".c") {
        writeLine(escapeCpp(openFence(ext1)));
        for (auto& l : content2) writeLine(l);
        writeLine(escapeCpp(closeFence(ext1)));

        writeLine("#if 0");
        for (auto& l : content1) writeLine(l);
        writeLine("#endif");
    }
    else {
        throw std::runtime_error("No C/C++ file in pair");
    }
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv, argv + argc);
    if (argc < 5) {
        std::cerr << usageStr;
        return 1;
    }
    bool verbose = false;
    std::string file1, file2, outFile;
    for (int i = 1; i < argc; i++) {
        if (args[i] == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -o requires an argument\n";
                return 1;
            }
            outFile = args[++i];
        } else if (file1.empty()) {
            file1 = args[i];
        } else if (file2.empty()) {
            file2 = args[i];
        } else if (args[i] == "-v" || args[i] == "--verbose") {
            verbose = true;
        } else {
            std::cerr << "Error: unexpected argument: " << args[i] << "\n";
            return 1;
        }
    }

    if (file1.empty() || file2.empty() || outFile.empty()) {
        std::cerr << usageStr;
        return 1;
    }

    std::string ext1 = fs::path(file1).extension().string();
    std::string ext2 = fs::path(file2).extension().string();

    if (verbose) std::cout << "Checking syntax for " << file1 << "... ";
    if (!checkSyntax(file1, ext1)) {
        std::cerr << "\nSyntax error in " << file1 << "\n";
        return 1;
    }
    if (verbose) std::cout << "OK\n";

    if (verbose) std::cout << "Checking syntax for " << file2 << "... ";
    if (!checkSyntax(file2, ext2)) {
        std::cerr << "\nSyntax error in " << file2 << "\n";
        return 1;
    }
    if (verbose) std::cout << "OK\n";

    auto content1 = readFile(file1);
    auto content2 = readFile(file2);

    writeMerged(outFile, ext1, content1, ext2, content2);
    if (verbose) std::cout << "Merged into " << outFile << "\n";
    return 0;
}
