import sys
import subprocess
from pathlib import Path
import argparse
import shlex

def run_cmd(cmd):
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        return result.stdout
    except Exception as e:
        return str(e)
    
def shell_safe(path: str) -> str:
    # Convert backslashes to forward slashes (for WSL/bash)
    p = Path(path).as_posix()
    # Quote the path safely for the shell
    return shlex.quote(p)

def check_syntax(file_path, ext) -> bool:
    file_str = str(file_path)
    if ext in ['.cpp', '.cc', '.cxx', '.c']:
        flag = " -x c " if ext == ".c" else ""
        res = run_cmd(f"g++ -fsyntax-only {flag} {file_str} 2>&1")
        if res.strip():
            print(f"C++ syntax errors in {file_str}:\n{res}")
            return False
        return True
    elif ext == '.py':
        res = run_cmd(f"python3 -m pyflakes {file_str} 2>&1")
        if res.strip(): res = run_cmd(f"python -m pyflakes {file_str} 2>&1")
        if res.strip():
            res = run_cmd(f"python3 -m py_compile {file_str} 2>&1")
            if res.strip(): res = run_cmd(f"python -m py_compile {file_str} 2>&1")
            if res.strip():
                print(f"Python syntax errors in {file_str}:\n{res}")
                print("If python file does not have syntax errors, please check if pyflakes is installed.")
                return False
        return True
    elif ext == '.rb':
        res = run_cmd(f"ruby -c {file_str} 2>&1")
        if "Syntax OK" not in res:
            print(f"Ruby syntax errors in {file_str}:\n{res}")
            return False
        return True
    elif ext == '.sh':
        res = run_cmd(f"bash -n {shell_safe(file_str)} 2>&1")
        if res.strip():
            print(f"Bash syntax errors in {file_str}:\n{res}")
            return False
        return True
    elif ext == ".pl":
        res = run_cmd(f"perl -c {file_str} 2>&1")
        if "syntax OK" not in res:
            print(f"Perl syntax errors in {file_str}:\n{res}")
            return False
        return True
    elif ext == ".vn":
        return True
    else:
        print(f"Unsupported file extension: {ext}")
        return False

verbose = None

def main():
    global verbose
    usage_str = """Usage: polyglot <source1> <source2> -o <outputFile>
Supported extensions:
  C/C++: .cpp, .cc, .cxx, .c
  Python: .py
  Ruby: .rb
  Bash: .sh
  Perl: .pl
  VastNova: .vn
"""
    
    parser = argparse.ArgumentParser(usage=usage_str)
    parser.add_argument('source1', help='First source file')
    parser.add_argument('source2', help='Second source file')
    parser.add_argument('-o', '--output', required=True, help='Output file')
    parser.add_argument('-v', '--verbose', help='verbose mode')
    args = parser.parse_args()

    file1 = Path(args.source1)
    file2 = Path(args.source2)
    out_file = Path(args.output)
    
    verbose = args.verbose

    ext1 = file1.suffix
    ext2 = file2.suffix

    # Check syntax
    if verbose: print(f"Checking syntax for {file1}... ", end='')
    if not check_syntax(file1, ext1):
        print(f"Syntax error in {file1}")
        return 1
    if verbose: print("OK")

    if verbose: print(f"Checking syntax for {file2}... ", end='')
    if not check_syntax(file2, ext2):
        print(f"Syntax error in {file2}")
        return 1
    if verbose: print("OK")

    # Read files
    with open(file1, 'r') as f:
        content1 = f.read().splitlines()
    with open(file2, 'r') as f:
        content2 = f.read().splitlines()

    # Determine fence tokens
    def open_fence(ext):
        if ext == '.py': return "r'''"
        if ext == '.rb': return "=begin"
        if ext == '.sh': return ": '"
        if ext == '.pl': return "=pod"
        if ext == ".vn": return "!#"
        return ""

    def close_fence(ext):
        if ext == '.py': return "'''"
        if ext == '.rb': return "=end"
        if ext == '.sh': return "'"
        if ext == ".pl": return "=cut"
        if ext == ".vn": return "#!"
        return ""

    # Write merged file
    with open(out_file, 'w', newline='\n') as f:
        if ext1 in ['.cpp', '.cc', '.cxx', '.c']:
            # Write C++ content with fence tokens for the other language
            f.write(f"#if 0\n{open_fence(ext2)}\n#endif\n")
            for line in content1:
                f.write(line + '\n')
            f.write(f"#if 0\n{close_fence(ext2)}\n#endif\n")
            
            # Write non-C++ content within #if 0 block
            f.write("#if 0\n")
            for line in content2:
                f.write(line + '\n')
            f.write("#endif\n")
            
        elif ext2 in ['.cpp', '.cc', '.cxx', '.c']:
            # Write C++ content with fence tokens for the other language
            f.write(f"#if 0\n{open_fence(ext1)}\n#endif\n")
            for line in content2:
                f.write(line + '\n')
            f.write(f"#if 0\n{close_fence(ext1)}\n#endif\n")
            
            # Write non-C++ content within #if 0 block
            f.write("#if 0\n")
            for line in content1:
                f.write(line + '\n')
            f.write("#endif\n")
            
        else:
            print("Error: No C/C++ file in pair")
            return 1

    if verbose: print(f"Merged into {out_file}")
    return 0

if __name__ == "__main__":
    sys.exit(main())