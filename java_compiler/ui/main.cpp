#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr int OPEN_ID = 101, SAVE_ID = 102, COMPILE_ID = 103;
HWND sourceBox = nullptr, outputBox = nullptr;
std::wstring currentFile;
std::wstring projectRoot, compilerPath;

std::string utf8Path(const std::wstring& path) {
    int count = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(count ? count : 0, '\0');
    if (count) WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &result[0], count, nullptr, nullptr);
    if (!result.empty()) result.pop_back();
    return result;
}

std::wstring readUtf8(const std::wstring& path) {
    std::ifstream input(utf8Path(path).c_str(), std::ios::binary);
    if (!input) return L"";
    std::string bytes((std::istreambuf_iterator<char>(input)), {});
    if (bytes.size() >= 3 && (unsigned char)bytes[0] == 0xEF &&
        (unsigned char)bytes[1] == 0xBB && (unsigned char)bytes[2] == 0xBF)
        bytes.erase(0, 3);
    if (bytes.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), &result[0], count);
    return result;
}

struct RuntimeValue {
    double number = 0;
    bool boolean = false;
};

RuntimeValue valueOf(const std::wstring& token, const std::map<std::wstring, RuntimeValue>& vars) {
    if (token == L"true") return {1, true};
    if (token == L"false") return {0, true};
    if (token.size() > 1 && token[0] == L'!')
        return {valueOf(token.substr(1), vars).number == 0 ? 1.0 : 0.0, true};
    auto found = vars.find(token);
    if (found != vars.end()) return found->second;
    wchar_t* end = nullptr;
    double number = wcstod(token.c_str(), &end);
    if (!end || *end != L'\0') throw std::runtime_error("Invalid TAC value: " + std::string(token.begin(), token.end()));
    return {number, false};
}

RuntimeValue evaluate(const std::vector<std::wstring>& parts,
                      const std::map<std::wstring, RuntimeValue>& vars) {
    if (parts.size() == 1) return valueOf(parts[0], vars);
    if (parts.size() == 2) {
        RuntimeValue value = valueOf(parts[1], vars);
        if (parts[0] == L"-") return {-value.number, false};
        if (parts[0] == L"!") return {value.number == 0 ? 1.0 : 0.0, true};
    }
    if (parts.size() != 3) throw std::runtime_error("Unsupported TAC expression.");
    RuntimeValue left = valueOf(parts[0], vars), right = valueOf(parts[2], vars);
    const std::wstring& op = parts[1];
    if (op == L"+") return {left.number + right.number, false};
    if (op == L"-") return {left.number - right.number, false};
    if (op == L"*") return {left.number * right.number, false};
    if (op == L"/") return {left.number / right.number, false};
    if (op == L"%") return {std::fmod(left.number, right.number), false};
    if (op == L"<") return {static_cast<double>(left.number < right.number), true};
    if (op == L">") return {static_cast<double>(left.number > right.number), true};
    if (op == L"<=") return {static_cast<double>(left.number <= right.number), true};
    if (op == L">=") return {static_cast<double>(left.number >= right.number), true};
    if (op == L"==") return {static_cast<double>(left.number == right.number), true};
    if (op == L"!=") return {static_cast<double>(left.number != right.number), true};
    if (op == L"&&") return {static_cast<double>(left.number != 0 && right.number != 0), true};
    if (op == L"||") return {static_cast<double>(left.number != 0 || right.number != 0), true};
    throw std::runtime_error("Unsupported TAC operator.");
}

std::wstring displayValue(RuntimeValue value) {
    if (value.boolean) return value.number != 0 ? L"true" : L"false";
    std::wostringstream output;
    if (std::floor(value.number) == value.number)
        output << (long long)value.number;
    else
        output << value.number;
    return output.str();
}

std::wstring runTac(const std::wstring& tac) {
    std::vector<std::wstring> lines;
    std::wistringstream input(tac);
    std::wstring line;
    while (std::getline(input, line)) if (!line.empty()) lines.push_back(line);
    std::map<std::wstring, size_t> labels;
    for (size_t i = 0; i < lines.size(); ++i)
        if (lines[i].back() == L':') labels[lines[i].substr(0, lines[i].size() - 1)] = i;

    std::map<std::wstring, RuntimeValue> vars;
    std::wstring output;
    for (size_t pc = 0, steps = 0; pc < lines.size() && steps < 1000000; ++pc, ++steps) {
        std::wistringstream row(lines[pc]);
        std::vector<std::wstring> words;
        std::wstring word;
        while (row >> word) words.push_back(word);
        if (words.empty() || words[0].back() == L':') continue;
        if (words[0] == L"goto" && words.size() == 2) {
            if (labels.count(words[1])) pc = labels[words[1]];
            continue;
        }
        if ((words[0] == L"if" || words[0] == L"ifFalse") && words.size() == 4) {
            bool condition = valueOf(words[1], vars).number != 0;
            if ((words[0] == L"if" && condition) || (words[0] == L"ifFalse" && !condition))
                if (labels.count(words[3])) pc = labels[words[3]];
            continue;
        }
        if (words[0] == L"print" && words.size() == 2) {
            if (!output.empty()) output += L"\r\n";
            output += displayValue(valueOf(words[1], vars));
            continue;
        }
        if (words.size() >= 3 && words[1] == L"=") {
            std::vector<std::wstring> expression(words.begin() + 2, words.end());
            vars[words[0]] = evaluate(expression, vars);
        }
    }
    return output;
}

bool writeUtf8(const std::wstring& path, const std::wstring& value) {
    int count = WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), nullptr, 0, nullptr, nullptr);
    std::string bytes(count, '\0');
    if (count) WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), &bytes[0], count, nullptr, nullptr);
    std::ofstream output(utf8Path(path).c_str(), std::ios::binary);
    output.write(bytes.data(), bytes.size());
    return (bool)output;
}

std::wstring controlText(HWND control) {
    int length = GetWindowTextLengthW(control);
    std::wstring value(length + 1, L'\0');
    GetWindowTextW(control, &value[0], length + 1);
    value.resize(length);
    return value;
}

void showOutput(const std::wstring& value) { SetWindowTextW(outputBox, value.c_str()); }

bool chooseFile(HWND window, bool save) {
    wchar_t path[MAX_PATH] = L"";
    if (!currentFile.empty()) lstrcpynW(path, currentFile.c_str(), MAX_PATH);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = L"Java source (*.java)\0*.java\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = L"java";
    dialog.Flags = save ? OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST : OFN_FILEMUSTEXIST;
    if (!(save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog))) return false;
    if (save) {
        if (!writeUtf8(path, controlText(sourceBox))) {
            MessageBoxW(window, L"Could not save the source file.", L"Save failed", MB_ICONERROR);
            return false;
        }
    } else {
        SetWindowTextW(sourceBox, readUtf8(path).c_str());
    }
    currentFile = path;
    return true;
}

void compileSource(HWND window) {
    const std::wstring inputPath = projectRoot + L"\\.compiler_ui_input.java";
    const std::wstring logPath = projectRoot + L"\\.compiler_ui_output.txt";
    const std::wstring tacPath = projectRoot + L"\\output.tac";
    if (!writeUtf8(inputPath, controlText(sourceBox))) {
        showOutput(L"Could not write temporary source file.");
        return;
    }
    SECURITY_ATTRIBUTES logSecurity{};
    logSecurity.nLength = sizeof(logSecurity);
    logSecurity.bInheritHandle = TRUE;
    HANDLE log = CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &logSecurity,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (log == INVALID_HANDLE_VALUE) {
        showOutput(L"Could not create the compiler log.");
        return;
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = log;
    startup.hStdError = log;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    std::wstring command = L"\"" + compilerPath + L"\" \"" + inputPath + L"\"";
    BOOL started = CreateProcessW(nullptr, &command[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                  nullptr, projectRoot.c_str(), &startup, &process);
    if (!started) {
        CloseHandle(log);
        showOutput(L"Could not start minijavac.exe. Build the project with `make ui`.");
        return;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(log);

    std::wstring report = readUtf8(logPath);
    if (exitCode == 0) {
        try {
            report = runTac(readUtf8(tacPath));
            if (report.empty()) report = L"(Program produced no output.)";
        } catch (const std::exception& error) {
            report = L"Could not run generated code: ";
            const std::string message = error.what();
            report.append(message.begin(), message.end());
        }
    } else {
        const std::string code = std::to_string(exitCode);
        report += L"\r\nCompilation failed (exit code " + std::wstring(code.begin(), code.end()) + L").";
    }
    showOutput(report);
    DeleteFileW(inputPath.c_str());
    DeleteFileW(logPath.c_str());
    (void)window;
}

void addLabel(HWND parent, const wchar_t* label, int x, int y) {
    CreateWindowW(L"STATIC", label, WS_CHILD | WS_VISIBLE, x, y, 400, 22,
                  parent, nullptr, nullptr, nullptr);
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        CreateWindowW(L"BUTTON", L"Open", WS_CHILD | WS_VISIBLE, 12, 10, 80, 30,
                      window, (HMENU)OPEN_ID, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE, 100, 10, 80, 30,
                      window, (HMENU)SAVE_ID, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Compile", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      188, 10, 100, 30, window, (HMENU)COMPILE_ID, nullptr, nullptr);
        addLabel(window, L"MiniJava source", 12, 50);
        sourceBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            L"class Example {\r\n    public static void main(String[] args) {\r\n        int x = 10;\r\n        System.out.println(x);\r\n    }\r\n}",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE |
            ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
            12, 74, 800, 300, window, nullptr, nullptr, nullptr);
        addLabel(window, L"Compiler output and generated TAC", 12, 386);
        outputBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            L"Ready. Compile to see diagnostics and TAC.",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE |
            ES_AUTOVSCROLL | ES_READONLY,
            12, 410, 800, 240, window, nullptr, nullptr, nullptr);
        return 0;
    case WM_SIZE: {
        int width = LOWORD(lParam), height = HIWORD(lParam);
        int sourceHeight = (height - 170) * 11 / 20;
        if (sourceHeight < 140) sourceHeight = 140;
        const int outputLabelY = 82 + sourceHeight + 8;
        const int outputTop = outputLabelY + 24;
        MoveWindow(sourceBox, 12, 74, width - 24, sourceHeight, TRUE);
        MoveWindow(outputBox, 12, outputTop, width - 24,
                   height - outputTop - 12 > 80 ? height - outputTop - 12 : 80, TRUE);
        HWND child = GetWindow(window, GW_CHILD);
        while (child) {
            wchar_t text[80]{};
            GetWindowTextW(child, text, 80);
            if (lstrcmpW(text, L"Compiler output and generated TAC") == 0)
                MoveWindow(child, 12, outputLabelY, 400, 22, TRUE);
            child = GetWindow(child, GW_HWNDNEXT);
        }
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == OPEN_ID) chooseFile(window, false);
        else if (LOWORD(wParam) == SAVE_ID) chooseFile(window, true);
        else if (LOWORD(wParam) == COMPILE_ID) compileSource(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    wchar_t module[MAX_PATH]{};
    GetModuleFileNameW(nullptr, module, MAX_PATH);
    std::wstring modulePath(module);
    size_t slash = modulePath.find_last_of(L"\\/");
    std::wstring buildDir = slash == std::wstring::npos ? L"." : modulePath.substr(0, slash);
    compilerPath = buildDir + L"\\minijavac.exe";
    size_t buildSlash = buildDir.find_last_of(L"\\/");
    projectRoot = buildSlash == std::wstring::npos ? L"." : buildDir.substr(0, buildSlash);
    buildSlash = projectRoot.find_last_of(L"\\/");
    projectRoot = buildSlash == std::wstring::npos ? L"." : projectRoot.substr(0, buildSlash);

    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"MiniJavaCompilerUI";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassW(&wc)) return 1;
    HWND window = CreateWindowW(wc.lpszClassName, L"MiniJava Compiler", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 860, 720, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return (int)message.wParam;
}
