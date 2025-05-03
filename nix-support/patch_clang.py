#!/usr/bin/env python3
import os
import sys
import argparse
import stat

def main():
    parser = argparse.ArgumentParser(
        description="Generate wrapper scripts that dispatch to different binaries based on '-stdlib=libc++' flag."
    )
    parser.add_argument("bash_path", help="Path to the shell interpreter (shebang)")
    parser.add_argument("out", help="Output directory")
    parser.add_argument("path1", help="First input path (should contain bin/)")
    parser.add_argument("path2", help="Second input path (should contain bin/)")
    args = parser.parse_args()

    bin1 = os.path.join(args.path1, "bin")
    bin2 = os.path.join(args.path2, "bin")
    out_bin = os.path.join(args.out, "bin")

    # create out/bin
    os.makedirs(out_bin, exist_ok=True)

    # list files in each bin directory
    try:
        files1 = sorted(
            f for f in os.listdir(bin1)
            if os.path.isfile(os.path.join(bin1, f))
        )
        files2 = sorted(
            f for f in os.listdir(bin2)
            if os.path.isfile(os.path.join(bin2, f))
        )
    except FileNotFoundError as e:
        sys.exit(f"Error: {e}")

    # ensure the filenames match
    set1, set2 = set(files1), set(files2)
    if set1 != set2:
        missing_in_1 = set2 - set1
        missing_in_2 = set1 - set2
        msg = ["Error: mismatched files between", bin1, "and", bin2]
        if missing_in_1:
            msg.append(f"\n  Missing in {bin1}: {sorted(missing_in_1)}")
        if missing_in_2:
            msg.append(f"\n  Missing in {bin2}: {sorted(missing_in_2)}")
        sys.exit("".join(msg))

    # for each file, generate a wrapper script
    for fname in files1:
        wrapper_path = os.path.join(out_bin, fname)
        with open(wrapper_path, "w") as w:
            w.write(f"#! {args.bash_path}\n")
            w.write("has_flag=false\n")
            w.write("for arg in \"$@\"; do\n")
            w.write("  if [[ \"$arg\" == *-stdlib=libc++* ]]; then\n")
            w.write("    has_flag=true\n")
            w.write("    break\n")
            w.write("  fi\n")
            w.write("done\n")
            w.write("if [ \"$has_flag\" = true ]; then\n")
            w.write(f"  exec \"{os.path.join(args.path2, 'bin', fname)}\" \"$@\"\n")
            w.write("else\n")
            w.write(f"  exec \"{os.path.join(args.path1, 'bin', fname)}\" \"$@\"\n")
            w.write("fi\n")

        # make the script executable
        st = os.stat(wrapper_path)
        os.chmod(wrapper_path, st.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

if __name__ == "__main__":
    main()
