PRE-COMMIT README
pre-commit is a hook made for windows-based commits using msys2 ucrt64.
It acts to run astyle and json_formatter on cpp or json files committed to src/ and data/ on the git repo.
The full scope of its reach is src/ (third-party/ excluded), tests/, tools/format/, tools/clang-tidy-plugin/, and .json files in data/.
If it fires, it will block the commit, reformat the files, and report what files it formatted to the console.
After optionally reviewing the files, restaging them will see them committed without issue.
Alternatively, running the commit with the --no-verify will skip the hook entirely.

To make this hook work on windows 10 with msys2 ucrt64:
Execute any msys2 terminal commands from within the git repo folder.
1: If you already have it, use pacman to uninstall astyle from msys2.
2: Download astyle 3.1 from sourceforge.net and extract it to a folder.
3: Copy the astyle.exe from the extracted zip.
4: Paste it to msys64\ucrt64\bin and rename it to "astyle.exe" instead of "AStyle.exe".
5: Run "astyle --version" in msys2 to make sure it works and prints "Artistic Style Version 3.1".
6: Build the json formatter by running "make tools/format/json_formatter.exe" and make sure a new exe is created.
7: Run "git config core.hooksPath tools/hooks" and then "git config core.hooksPath" to see if it returns "tools/hooks".

After this, the pre-commit hook should run every time you commit on windows.
It should also work on linux and macOS, but the setup will differ for installing astyle and building the json_formatter.