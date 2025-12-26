#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <utility>
#include <cstdio>
#include <cstdlib>
#if defined(__APPLE__) || defined(__linux__)
#include <sys/wait.h> // for WIFEXITED, WEXITSTATUS
#endif
#include <unistd.h>

using namespace std;

struct CommandResult {
    int exitCode;
    string output;
};

static string esc_reset = "\033[0m";
static string esc_red   = "\033[31m";
static string esc_green = "\033[32m";
static string esc_yellow= "\033[33m";
static string esc_blue  = "\033[34m";
static string esc_magenta = "\033[35m";
static string esc_bold  = "\033[1m";

CommandResult runCommand(const std::string& cmd) {
    // add logging
    // std::cout << esc_magenta << "\tCmd: " << cmd << esc_reset << std::endl;

    std::array<char, 256> buffer;
    std::string result;

    std::string shellCmd = cmd + " 2>&1";
    FILE* pipe = popen(shellCmd.c_str(), "r");
    if (!pipe) {
        return { -1, "popen() failed for: " + cmd };
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int rc = pclose(pipe);
#if defined(__unix__) || defined(__APPLE__)
    if (WIFEXITED(rc)) rc = WEXITSTATUS(rc);
#endif
    return { rc, result };
}

struct TestCase {
    string name;
    string generatorCmd;
    string generatedFile;
    string compileCmd;
    string compiledExe;
    vector<pair<string,string>> runSteps;
};

struct TestResult {
    string name;
    bool passed;
    vector<pair<string, CommandResult>> stepResults;
};

static void printHeader(const string &title) {
    cout << esc_bold << title << esc_reset << "\n";
}

#ifdef _WIN32
    const std::string exeSuffix = ".exe";
    const std::string exePrefix = ".\\";
#else
    const std::string exeSuffix = "";
    const std::string exePrefix = "./";
#endif

// Normalize paths: on Windows replace '\' with '/' for bash/WSL compatibility
static string normalizePath(string p) {
#if defined(_WIN32)
    for (auto &c : p) if (c == '\\') c = '/';
#endif
    return p;
}

int main() {
    string testDir = normalizePath("./test");

    printHeader("--- Polyglot test runner ---");
    cout << "Test directory: " << testDir << "\n\n";

    // Build polyglot
    cout << esc_blue << "Step: build polyglot from main.cpp (g++ main.cpp -o polyglot)" << esc_reset << "\n";
    auto buildPoly = runCommand("g++ main.cpp -std=c++17 -o polyglot");
    if (buildPoly.exitCode == 0) {
        cout << esc_green << "Built polyglot OK\n" << esc_reset;
    } else {
        cout << esc_yellow << "Warning: build returned " << buildPoly.exitCode << "\n"
             << buildPoly.output << esc_reset << "\n";
    }

    auto mkCompile = [](const string &compiler, const string &input, const string &outputExe) {
        return compiler + " " + input + " -o " + outputExe + exeSuffix;
    };

    vector<TestCase> tests;

    // Generators: compiled binary and python
    vector<pair<string,string>> generators = {
        {exePrefix + "polyglot" + exeSuffix, "C++ binary"},
        {"python main.py", "Python script"}
    };

    struct PairDef {
        string a,b,genFile,compiler,exePath,runCompiledCmd,runScriptCmd;
    };

    vector<PairDef> combos_cpp_style = {
        {testDir + "/test.cpp", testDir + "/test.py", testDir + "/out.cpp", "g++", testDir + "/out", exePrefix + "out" + exeSuffix, "python " + testDir + "/out.cpp"},
        {testDir + "/test.py",  testDir + "/test.cpp", testDir + "/out.cpp", "g++", testDir + "/out", exePrefix + "out" + exeSuffix, "python " + testDir + "/out.cpp"},
        {testDir + "/test.cpp", testDir + "/test.rb", testDir + "/out.cpp", "g++", testDir + "/out", exePrefix + "out" + exeSuffix, "ruby "   + testDir + "/out.cpp"},
        {testDir + "/test.rb",  testDir + "/test.cpp", testDir + "/out.cpp", "g++", testDir + "/out", exePrefix + "out" + exeSuffix, "ruby "   + testDir + "/out.cpp"},
        {testDir + "/test.cpp", testDir + "/test.sh", testDir + "/out.cpp", "g++", testDir + "/out", exePrefix + "out" + exeSuffix, "bash "   + testDir + "/out.cpp"},
        {testDir + "/test.sh",  testDir + "/test.cpp", testDir + "/out.cpp", "g++", testDir + "/out", exePrefix + "out" + exeSuffix, "bash "   + testDir + "/out.cpp"},
        {testDir + "/test.pl",  testDir + "/test.cpp", testDir + "/out.cpp", "g++", testDir + "/out",
        exePrefix + "out" + exeSuffix, "perl "   + testDir + "/out.cpp"},
        {testDir + "/test.vn", testDir + "/test.cpp", testDir + "/out.cpp", "g++",
        testDir + "/out"},
        {testDir + "/test.vn", testDir + "/test.cpp", testDir + "/out.cpp", "g++",
        testDir + "/out", exePrefix + "out" + exeSuffix, "vastnova " + testDir + "/out.cpp"}
    };

    vector<PairDef> combos_c_style = {
        {testDir + "/test.c", testDir + "/test.py", testDir + "/out.c", "gcc", testDir + "/out", exePrefix + "out" + exeSuffix, "python " + testDir + "/out.c"},
        {testDir + "/test.py", testDir + "/test.c", testDir + "/out.c", "gcc", testDir + "/out", exePrefix + "out" + exeSuffix, "python " + testDir + "/out.c"},
        {testDir + "/test.c", testDir + "/test.rb", testDir + "/out.c", "gcc", testDir + "/out", exePrefix + "out" + exeSuffix, "ruby "   + testDir + "/out.c"},
        {testDir + "/test.rb", testDir + "/test.c", testDir + "/out.c", "gcc", testDir + "/out", exePrefix + "out" + exeSuffix, "ruby "   + testDir + "/out.c"},
        {testDir + "/test.c", testDir + "/test.sh", testDir + "/out.c", "gcc", testDir + "/out", exePrefix + "out" + exeSuffix, "bash "   + testDir + "/out.c"},
        {testDir + "/test.sh", testDir + "/test.c", testDir + "/out.c", "gcc", testDir + "/out", exePrefix + "out" + exeSuffix, "bash "   + testDir + "/out.c"},
        {testDir + "/test.pl", testDir + "/test.c", testDir + "/out.c", "gcc", testDir + "/out", exePrefix + "out" + exeSuffix, "perl "   + testDir + "/out.c"},
        {testDir + "/test.vn", testDir + "/test.c", testDir + "/out.c", "gcc",
        testDir + "/out", exePrefix + "out" + exeSuffix, "vastnova " + testDir + "/out.c"}
    };

    // Build tests
    for (auto &g : generators) {
        for (auto &p : combos_cpp_style) {
            TestCase t;
            t.name = g.second + " : " + (p.a.substr(p.a.find_last_of('/')+1)) + " + " + (p.b.substr(p.b.find_last_of('/')+1));
            t.generatorCmd = g.first + " " + normalizePath(p.a) + " " + normalizePath(p.b) + " -o " + normalizePath(p.genFile);
            t.generatedFile = normalizePath(p.genFile);
            t.compileCmd = mkCompile(p.compiler, normalizePath(p.genFile), normalizePath(p.exePath));
            t.compiledExe = normalizePath(p.exePath) + exeSuffix;
            t.runSteps.push_back({"run-compiled", p.runCompiledCmd});
            t.runSteps.push_back({"run-interpreter", p.runScriptCmd});
            tests.push_back(std::move(t));
        }
    }

    for (auto &g : generators) {
        for (auto &p : combos_c_style) {
            TestCase t;
            t.name = g.second + " : " + (p.a.substr(p.a.find_last_of('/')+1)) + " + " + (p.b.substr(p.b.find_last_of('/')+1));
            t.generatorCmd = g.first + " " + normalizePath(p.a) + " " + normalizePath(p.b) + " -o " + normalizePath(p.genFile);
            t.generatedFile = normalizePath(p.genFile);
            t.compileCmd = mkCompile(p.compiler, normalizePath(p.genFile), normalizePath(p.exePath));
            t.compiledExe = normalizePath(p.exePath) + exeSuffix;
            t.runSteps.push_back({"run-compiled", p.runCompiledCmd});
            t.runSteps.push_back({"run-interpreter", p.runScriptCmd});
            tests.push_back(std::move(t));
        }
    }

    // Run tests
    vector<TestResult> results;
    for (size_t i = 0; i < tests.size(); ++i) {
        auto &tc = tests[i];
        cout << esc_blue << "=== TEST " << (i+1) << "/" << tests.size() << " : " << tc.name << " ===" << esc_reset << "\n";

        TestResult tr{tc.name, true, {}};

        cout << "-> generator: " << tc.generatorCmd << "\n";
        auto genRes = runCommand(tc.generatorCmd);
        tr.stepResults.push_back({"generator", genRes});
        if (genRes.exitCode != 0) {
            cout << esc_red << "generator FAILED\n" << genRes.output << esc_reset << "\n";
            tr.passed = false;
            results.push_back(tr);
            continue;
        }

        cout << esc_green << "generator OK\n" << esc_reset;

        cout << "-> compile: " << tc.compileCmd << "\n";
        auto compRes = runCommand(tc.compileCmd);
        tr.stepResults.push_back({"compile", compRes});
        if (compRes.exitCode != 0) {
            cout << esc_red << "compile FAILED\n" << compRes.output << esc_reset << "\n";
            tr.passed = false;
            results.push_back(tr);
            continue;
        }
        cout << esc_green << "compile OK\n" << esc_reset;

        // On Unix-like systems, ensure the compiled executable is executable
#if !defined(_WIN32)
        {
            string chmodCmd = "chmod +x " + tc.compiledExe;
            auto chmodRes = runCommand(chmodCmd);
            if (chmodRes.exitCode != 0) {
                cout << esc_yellow << "chmod warning: " << chmodRes.output << esc_reset << "\n";
            }

            // Check file exists and show details (helpful for debugging "not found")
            string checkFileCmd = "ls -la " + tc.compiledExe + " 2>/dev/null || echo \"File not found\"";
            auto checkRes = runCommand(checkFileCmd);
            cout << "File check: " << checkRes.output << "\n";
        }
#endif

        for (auto &rs : tc.runSteps) {
            // Resolve the run command. For the compiled step, use the actual compiled executable
            string runCmd = rs.second;
            if (rs.first == "run-compiled") {
                string compiled = tc.compiledExe;
#if defined(_WIN32)
                // Convert forward slashes to backslashes for Windows shell
                for (auto &c : compiled) if (c == '/') c = '\\';
                // If compiled path is relative and doesn't already start with .\, prefix it
                if (compiled.rfind(".\\", 0) != 0 && compiled.find(":") == string::npos && compiled.rfind("\\\\", 0) != 0) {
                    compiled = exePrefix + compiled;
                }
#else
                // On Unix, ensure relative executables are prefixed with ./ for direct execution
                if (compiled.rfind("./", 0) != 0 && compiled.size() > 0 && compiled[0] != '/') {
                    compiled = exePrefix + compiled;
                }
#endif
                runCmd = compiled;
            }

            cout << "-> " << rs.first << ": " << runCmd << "\n";
            auto rr = runCommand(runCmd);
            tr.stepResults.push_back({rs.first, rr});
            if (rr.exitCode != 0) {
                cout << esc_red << rs.first << " FAILED\n" << rr.output << esc_reset << "\n";
                tr.passed = false;
                break;
            }
            cout << esc_green << rs.first << " OK\n" << esc_reset;
        }

        results.push_back(tr);
    }

    // Summary
    int passed = 0, failed = 0;
    for (auto &r : results) (r.passed ? ++passed : ++failed);

    cout << esc_bold << "\n=== SUMMARY ===" << esc_reset << "\n";
    cout << "Total tests: " << results.size() << ", " << esc_green << "PASSED: " << passed
         << esc_reset << ", " << (failed ? esc_red : esc_green) << "FAILED: " << failed << esc_reset << "\n";

    return failed == 0 ? 0 : 2;
}
