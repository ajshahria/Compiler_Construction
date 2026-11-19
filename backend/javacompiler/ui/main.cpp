#define WIN32_LEAN_AND_MEAN
#define _WIN32_IE 0x0600
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
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
constexpr int OPEN_ID = 101, SAVE_ID = 102, COMPILE_ID = 103, THEME_ID = 104, TABS_ID = 105;
HWND sourceBox = nullptr, outputBox = nullptr, tabs = nullptr;
HWND themeButton = nullptr, mainWindow = nullptr;
bool darkMode = false;
HBRUSH darkBrush = CreateSolidBrush(RGB(30,32,51));
HBRUSH lightBrush = CreateSolidBrush(RGB(255,255,255));
std::wstring tabText[6];
const wchar_t* tabNames[6] = {L"Tokens", L"AST", L"Symbol Table", L"TAC", L"Output", L"Error"};
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
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (!line.empty()) lines.push_back(line);
    }
    std::map<std::wstring, size_t> labels;
    for (size_t i = 0; i < lines.size(); ++i)
        if (lines[i].back() == L':') labels[lines[i].substr(0, lines[i].size() - 1)] = i;

    std::map<std::wstring, RuntimeValue> vars;
    std::wstring output;
    size_t pc = 0;
    size_t steps = 0;
    while (pc < lines.size() && steps++ < 1000000) {
        std::wistringstream row(lines[pc]);
        std::vector<std::wstring> words;
        std::wstring word;
        while (row >> word) words.push_back(word);
        if (words.empty() || words[0].back() == L':') { ++pc; continue; }
        if (words[0] == L"goto" && words.size() == 2) {
            auto target = labels.find(words[1]);
            if (target == labels.end()) throw std::runtime_error("Unknown TAC label.");
            pc = target->second + 1;
            continue;
        }
        if ((words[0] == L"if" || words[0] == L"ifFalse") && words.size() == 4) {
            bool condition = valueOf(words[1], vars).number != 0;
            if ((words[0] == L"if" && condition) || (words[0] == L"ifFalse" && !condition)) {
                auto target = labels.find(words[3]);
                if (target == labels.end()) throw std::runtime_error("Unknown TAC label.");
                pc = target->second + 1;
            } else {
                ++pc;
            }
            continue;
        }
        if (words[0] == L"print" && words.size() == 2) {
            if (!output.empty()) output += L"\r\n";
            output += displayValue(valueOf(words[1], vars));
            ++pc;
            continue;
        }
        if (words.size() >= 3 && words[1] == L"=") {
            std::vector<std::wstring> expression(words.begin() + 2, words.end());
            vars[words[0]] = evaluate(expression, vars);
        }
        ++pc;
    }
    if (steps >= 1000000) throw std::runtime_error("Execution stopped after 1,000,000 TAC instructions.");
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

std::wstring tokenReport(const std::wstring& source) {
    std::wstring result;
    for (size_t i=0; i<source.size();) {
        if (iswspace(source[i])) { ++i; continue; }
        if (source[i]==L'/' && i+1<source.size() && source[i+1]==L'/') { while(i<source.size()&&source[i]!=L'\n')++i; continue; }
        size_t start=i;
        if (iswalnum(source[i]) || source[i]==L'_' || source[i]==L'$') {
            bool number=iswdigit(source[i]); ++i;
            while(i<source.size() && (iswalnum(source[i])||source[i]==L'_'||source[i]==L'$'||(number&&source[i]==L'.'))) ++i;
        } else if (i+1<source.size() && ((source[i]==L'='&&source[i+1]==L'=')||(source[i]==L'!'&&source[i+1]==L'=')||(source[i]==L'<'&&source[i+1]==L'=')||(source[i]==L'>'&&source[i+1]==L'=')||(source[i]==L'&'&&source[i+1]==L'&')||(source[i]==L'|'&&source[i+1]==L'|'))) i+=2;
        else ++i;
        result += source.substr(start,i-start)+L"\r\n";
    }
    return result;
}

void selectTab(int index) { if(index>=0 && index<6) showOutput(tabText[index]); }

void applyTheme() {
    COLORREF back = darkMode ? RGB(30,32,51) : RGB(255,255,255);
    for (HWND child = GetWindow(mainWindow, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        RedrawWindow(child, nullptr, nullptr, RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
    }
    SetWindowTextW(themeButton, darkMode ? L"Light mode" : L"Dark mode");
    SetClassLongPtrW(mainWindow, GCLP_HBRBACKGROUND, (LONG_PTR)(darkMode ? darkBrush : lightBrush));
    RedrawWindow(mainWindow, nullptr, nullptr, RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}

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
    const std::wstring source = controlText(sourceBox);
    tabText[0] = tokenReport(source);
    tabText[1] = L"AST will be available after parsing.";
    tabText[2] = L"Symbol table will be available after semantic analysis.";
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
    std::wstring command = L"\"" + compilerPath + L"\" --emit-files \"" + inputPath + L"\"";
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
            tabText[1] = readUtf8(projectRoot + L"\\ast.txt");
            tabText[2] = readUtf8(projectRoot + L"\\symbols.txt");
            tabText[3] = readUtf8(tacPath);
            tabText[4] = runTac(tabText[3]);
            if (tabText[4].empty()) tabText[4] = L"(Program produced no output.)";
            tabText[5] = L"No errors.";
        } catch (const std::exception& error) {
            tabText[4] = L"Could not run generated code: ";
            const std::string message = error.what();
            tabText[4].append(message.begin(), message.end());
            tabText[5] = tabText[4];
        }
    } else {
        tabText[5] = report;
        const std::string code = std::to_string(exitCode);
        tabText[5] += L"\r\nCompilation failed (exit code " + std::wstring(code.begin(), code.end()) + L").";
    }
    selectTab(TabCtrl_GetCurSel(tabs));
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
        themeButton = CreateWindowW(L"BUTTON", L"Dark mode", WS_CHILD | WS_VISIBLE,
                      700, 10, 120, 30, window, (HMENU)THEME_ID, nullptr, nullptr);
        addLabel(window, L"MiniJava source", 12, 50);
        sourceBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            L"class Example {\r\n    public static void main(String[] args) {\r\n        int x = 10;\r\n        System.out.println(x);\r\n    }\r\n}",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE |
            ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
            12, 74, 800, 300, window, nullptr, nullptr, nullptr);
        tabs = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|TCS_FIXEDWIDTH,
            0,0,0,0,window,(HMENU)TABS_ID,nullptr,nullptr);
        SendMessageW(tabs, TCM_SETITEMSIZE, 0, MAKELPARAM(92, 28));
        for(int i=0;i<6;++i){
            TCITEMW item{};
            item.mask=TCIF_TEXT;
            item.pszText=(LPWSTR)tabNames[i];
            item.cchTextMax=(int)wcslen(tabNames[i])+1;
            SendMessageW(tabs, TCM_INSERTITEMW, (WPARAM)i, (LPARAM)&item);
        }
        outputBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Compile to view analysis results.",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE |
            ES_AUTOVSCROLL | ES_READONLY,
            12, 410, 800, 240, tabs, nullptr, nullptr, nullptr);
        return 0;
    case WM_SIZE: {
        int width = LOWORD(lParam), height = HIWORD(lParam);
        int sourceHeight = (height - 170) * 11 / 20;
        if (sourceHeight < 140) sourceHeight = 140;
        const int outputLabelY = 82 + sourceHeight + 8;
        int editorWidth = (width-36)/2;
        MoveWindow(sourceBox, 12, 74, editorWidth, height-88, TRUE);
        MoveWindow(tabs, 24+editorWidth, 50, width-editorWidth-36, height-62, TRUE);
        MoveWindow(themeButton, width - 132, 10, 120, 30, TRUE);
        RECT r{}; GetClientRect(tabs,&r); TabCtrl_AdjustRect(tabs,FALSE,&r);
        MoveWindow(outputBox,r.left+6,r.top+6,r.right-r.left-12,r.bottom-r.top-12,TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == OPEN_ID) chooseFile(window, false);
        else if (LOWORD(wParam) == SAVE_ID) chooseFile(window, true);
        else if (LOWORD(wParam) == COMPILE_ID) compileSource(window);
        else if (LOWORD(wParam) == THEME_ID) { darkMode = !darkMode; applyTheme(); }
        return 0;
    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->idFrom==TABS_ID && ((LPNMHDR)lParam)->code==TCN_SELCHANGE) selectTab(TabCtrl_GetCurSel(tabs));
        return 0;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)wParam;
        SetTextColor(dc, darkMode ? RGB(242,243,250) : RGB(20,20,20));
        SetBkColor(dc, darkMode ? RGB(30,32,51) : RGB(255,255,255));
        return (LRESULT)(darkMode ? darkBrush : lightBrush);
    }
    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(window,&rc);
        FillRect((HDC)wParam,&rc,darkMode?darkBrush:lightBrush);
        return 1;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    (void)applyTheme;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_TAB_CLASSES}; InitCommonControlsEx(&controls);
    wchar_t module[MAX_PATH]{};
    GetModuleFileNameW(nullptr, module, MAX_PATH);
    std::wstring modulePath(module);
    size_t slash = modulePath.find_last_of(L"\\/");
    std::wstring buildDir = slash == std::wstring::npos ? L"." : modulePath.substr(0, slash);
    compilerPath = buildDir + L"\\minijavac.exe";
    // The executables live in <project>/build, so the project root is one level up.
    size_t buildSlash = buildDir.find_last_of(L"\\/");
    projectRoot = buildSlash == std::wstring::npos ? L"." : buildDir.substr(0, buildSlash);

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
    mainWindow = window;
    ShowWindow(window, show);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return (int)message.wParam;
}
