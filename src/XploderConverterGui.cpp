// XploderConverterGui.cpp
// Native Win32 side-by-side GUI for the Xploder PSX CMP converter.
// Build with build-gui-msvc.cmd from a Visual Studio Developer Command Prompt.

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "MultiFormatCodeConverter.hpp"

#if __has_include("resource.h")
#include "resource.h"
#endif

#ifndef IDI_APP_ICON
#define IDI_APP_ICON 101
#endif

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shell32.lib")

namespace
{
    constexpr wchar_t WindowClassName[] = L"XploderPsxConverterGuiWindow";
    constexpr wchar_t SplitterClassName[] = L"XploderPsxConverterSplitter";
    constexpr wchar_t AppTitle[] = L"Xploder PSX Converter";
    constexpr wchar_t AppVersion[] = L"v1.03";
    constexpr wchar_t WindowTitle[] = L"Xploder PSX Converter v1.03";

    enum ControlId : int
    {
        IdInputEdit = 1001,
        IdOutputEdit,
        IdInputTypeCombo,
        IdOutputTypeCombo,
        IdKeyCombo,
        IdPayloadKeyCombo,
        IdGroupCheck,
        IdAnnotateCheck,
        IdCmpOutputCheck,
        IdDuckStationCombineCheck,
        IdDuckStationCondenseCheck,
        IdSerialRepeaterCondenseCheck,
        IdAutoCheck,
        IdConvertButton,
        IdCopyButton,
        IdClearButton,
        IdSwapButton,
        IdSplitter,
        IdProgressBar,
        IdStatusLabel
    };

    enum MenuId : int
    {
        IdMenuAutoConvert = 3001,
        IdMenuAnnotateCodeTypes,
        IdMenuCmpDbCompatible,

        IdMenuHideConvert,
        IdMenuHideCopyOutput,
        IdMenuHideOutputToInput,
        IdMenuHideClear,

        IdMenuGroupEncrypted,
        IdMenuEncryptionKey4,
        IdMenuEncryptionKey5,
        IdMenuEncryptionKey6,
        IdMenuEncryptionKey7,
        IdMenuPayloadKey6,
        IdMenuPayloadKey7,

        IdMenuDuckStationCombine,
        IdMenuDuckStationCondense,
        IdMenuSerialRepeaterCondense
    };

    HWND g_mainWindow = nullptr;
    HWND g_inputEdit = nullptr;
    HWND g_outputEdit = nullptr;
    HWND g_inputTypeCombo = nullptr;
    HWND g_outputTypeCombo = nullptr;
    HWND g_keyCombo = nullptr;
    HWND g_payloadKeyCombo = nullptr;
    HWND g_groupCheck = nullptr;
    HWND g_annotateCheck = nullptr;
    HWND g_cmpOutputCheck = nullptr;
    HWND g_duckStationCombineCheck = nullptr;
    HWND g_duckStationCondenseCheck = nullptr;
    HWND g_serialRepeaterCondenseCheck = nullptr;
    HWND g_autoCheck = nullptr;
    HWND g_convertButton = nullptr;
    HWND g_copyButton = nullptr;
    HWND g_clearButton = nullptr;
    HWND g_swapButton = nullptr;
    HWND g_statusLabel = nullptr;
    HWND g_progressBar = nullptr;
    HWND g_splitter = nullptr;
    HWND g_inputLabel = nullptr;
    HWND g_outputLabel = nullptr;
    HMENU g_menuBar = nullptr;
    HMENU g_optionsMenu = nullptr;
    HMENU g_programOptionsMenu = nullptr;
    HMENU g_hideButtonsMenu = nullptr;
    HMENU g_currentOutputMenu = nullptr;
    HMENU g_encryptionKeyMenu = nullptr;
    HMENU g_payloadKeyMenu = nullptr;
    HMENU g_autoConversionMenu = nullptr;

    HHOOK g_optionsMenuMessageHook = nullptr;
    HMENU g_selectedMenu = nullptr;
    UINT g_selectedMenuItem = 0U;
    UINT g_selectedMenuFlags = 0U;
    HFONT g_uiFont = nullptr;
    bool g_isConverting = false;
    bool g_isLoadingInput = false;
    bool g_isDraggingSplitter = false;
    bool g_isBatchDecrypting = false;
    bool g_hideConvertButton = false;
    bool g_hideCopyButton = false;
    bool g_hideSwapButton = false;
    bool g_hideClearButton = false;
    bool g_pendingButtonVisibilityUpdate = false;
    double g_splitRatio = 0.5;

    constexpr int LayoutMargin = 10;
    constexpr int SplitterWidth = 8;
    constexpr int MinimumPaneWidth = 100;

    void layoutControls(HWND hwnd);
    void layoutPaneControls(HWND hwnd, bool repaintImmediately);
    void convertNow();
    void updateModeUi();
    void updateOptionsMenu();
    void refreshOptionsMenuChecksOnly();
    bool handleOptionsMenuCommand(int id, bool preserveOpenMenus);
    bool isOptionsMenuCommand(int id);
    int selectedKeyboardMenuCommand();
    void redrawOpenPopupMenusImmediately();
    LRESULT CALLBACK optionsMenuMessageFilter(int code, WPARAM wParam, LPARAM lParam);
    void savePersistentSettings();
    psx_code_types::Family selectedFamily(HWND combo);
    bool batchDecryptFolder(const std::wstring& folderPath);
    void setBatchUiActive(bool active);
    void updateBatchProgress(std::size_t completed, std::size_t total, const std::wstring& currentFile);

    std::wstring widenUtf8(const std::string& text)
    {
        if (text.empty())
            return {};

        const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (needed <= 0)
        {
            // Fallback for legacy single-byte text.
            const int ansiNeeded = MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
            if (ansiNeeded <= 0)
                return {};
            std::wstring out(static_cast<std::size_t>(ansiNeeded), L'\0');
            MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), out.data(), ansiNeeded);
            return out;
        }

        std::wstring out(static_cast<std::size_t>(needed), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(), needed);
        return out;
    }

    std::string narrowUtf8(const std::wstring& text)
    {
        if (text.empty())
            return {};

        const int needed = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (needed <= 0)
            return {};

        std::string out(static_cast<std::size_t>(needed), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), needed, nullptr, nullptr);
        return out;
    }

    std::wstring getWindowText(HWND hwnd)
    {
        const int length = GetWindowTextLengthW(hwnd);
        if (length <= 0)
            return {};

        std::wstring text(static_cast<std::size_t>(length) + 1U, L'\0');
        const int copied = GetWindowTextW(hwnd, text.data(), length + 1);
        text.resize(static_cast<std::size_t>(std::max(0, copied)));
        return text;
    }

    void setWindowText(HWND hwnd, const std::wstring& text)
    {
        SetWindowTextW(hwnd, text.c_str());
    }


    std::wstring normalizeLineEndingsForEdit(const std::wstring& text)
    {
        std::wstring out;
        out.reserve(text.size() + 16U);

        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const wchar_t ch = text[i];
            if (ch == L'\r')
            {
                out.push_back(L'\r');
                out.push_back(L'\n');
                if (i + 1U < text.size() && text[i + 1U] == L'\n')
                    ++i;
                continue;
            }

            // Browser/chat clipboard text can use LF-only or Unicode line separators.
            if (ch == L'\n' || ch == L'\v' || ch == L'\f' ||
                ch == static_cast<wchar_t>(0x0085) ||
                ch == static_cast<wchar_t>(0x2028) ||
                ch == static_cast<wchar_t>(0x2029))
            {
                out.push_back(L'\r');
                out.push_back(L'\n');
                continue;
            }

            out.push_back(ch);
        }

        return out;
    }

    void setEditText(HWND hwnd, const std::wstring& text)
    {
        setWindowText(hwnd, normalizeLineEndingsForEdit(text));
    }

    bool pasteNormalizedClipboardText(HWND hwnd)
    {
        if (!OpenClipboard(hwnd))
            return false;

        std::wstring clipboardText;

        if (HANDLE unicodeHandle = GetClipboardData(CF_UNICODETEXT))
        {
            const auto* unicodeText = static_cast<const wchar_t*>(GlobalLock(unicodeHandle));
            if (unicodeText != nullptr)
            {
                clipboardText.assign(unicodeText);
                GlobalUnlock(unicodeHandle);
            }
        }
        else if (HANDLE ansiHandle = GetClipboardData(CF_TEXT))
        {
            const auto* ansiText = static_cast<const char*>(GlobalLock(ansiHandle));
            if (ansiText != nullptr)
            {
                const int needed = MultiByteToWideChar(CP_ACP, 0, ansiText, -1, nullptr, 0);
                if (needed > 1)
                {
                    std::wstring converted(static_cast<std::size_t>(needed), L'\0');
                    MultiByteToWideChar(CP_ACP, 0, ansiText, -1, converted.data(), needed);
                    converted.resize(static_cast<std::size_t>(needed - 1));
                    clipboardText = converted;
                }
                GlobalUnlock(ansiHandle);
            }
        }

        CloseClipboard();

        if (clipboardText.empty())
            return false;

        const std::wstring normalized = normalizeLineEndingsForEdit(clipboardText);
        SendMessageW(hwnd, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(normalized.c_str()));
        return true;
    }

    bool handleEditShortcut(HWND hwnd, UINT msg, WPARAM wParam, bool allowPaste)
    {
        if (msg != WM_KEYDOWN || (GetKeyState(VK_CONTROL) & 0x8000) == 0)
            return false;

        switch (wParam)
        {
            case L'A':
                SendMessageW(hwnd, EM_SETSEL, 0, -1);
                return true;

            case L'C':
                SendMessageW(hwnd, WM_COPY, 0, 0);
                return true;

            case L'V':
                if (allowPaste)
                {
                    // Insert the normalized clipboard text directly. Sending WM_PASTE
                    // here and then allowing TranslateMessage's Ctrl+V WM_CHAR (0x16)
                    // to reach the EDIT control can make the same text paste twice.
                    pasteNormalizedClipboardText(hwnd);
                    return true;
                }
                break;
        }

        return false;
    }

    bool isHandledEditShortcutCharacter(UINT msg, WPARAM wParam)
    {
        if (msg != WM_CHAR)
            return false;

        // TranslateMessage still posts these control characters even when the
        // corresponding WM_KEYDOWN was handled by our editor subclass.
        return wParam == 0x01U || // Ctrl+A
               wParam == 0x03U || // Ctrl+C
               wParam == 0x16U;   // Ctrl+V
    }

    bool readDroppedFileBytes(
        const std::wstring& path,
        std::vector<std::uint8_t>& bytes,
        std::wstring& error)
    {
        constexpr LONGLONG MaximumDroppedFileSize = 64LL * 1024LL * 1024LL;

        HANDLE file = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr);

        if (file == INVALID_HANDLE_VALUE)
        {
            error = L"Windows could not open the file.";
            return false;
        }

        LARGE_INTEGER fileSize{};
        if (!GetFileSizeEx(file, &fileSize))
        {
            CloseHandle(file);
            error = L"Windows could not determine the file size.";
            return false;
        }

        if (fileSize.QuadPart < 0 || fileSize.QuadPart > MaximumDroppedFileSize)
        {
            CloseHandle(file);
            error = L"The file is larger than the 64 MB text-file limit.";
            return false;
        }

        bytes.resize(static_cast<std::size_t>(fileSize.QuadPart));
        std::size_t totalRead = 0;
        while (totalRead < bytes.size())
        {
            const std::size_t remaining = bytes.size() - totalRead;
            const DWORD request = static_cast<DWORD>(std::min<std::size_t>(remaining, 1024U * 1024U));
            DWORD amountRead = 0;
            if (!ReadFile(file, bytes.data() + totalRead, request, &amountRead, nullptr))
            {
                CloseHandle(file);
                bytes.clear();
                error = L"Windows could not read the file.";
                return false;
            }

            if (amountRead == 0)
                break;

            totalRead += amountRead;
        }

        CloseHandle(file);
        bytes.resize(totalRead);
        return true;
    }

    std::wstring decodeDroppedText(const std::vector<std::uint8_t>& bytes)
    {
        if (bytes.empty())
            return {};

        // UTF-16 little-endian BOM.
        if (bytes.size() >= 2U && bytes[0] == 0xFFU && bytes[1] == 0xFEU)
        {
            const std::size_t characterCount = (bytes.size() - 2U) / 2U;
            std::wstring text(characterCount, L'\0');
            for (std::size_t i = 0; i < characterCount; ++i)
            {
                const std::size_t offset = 2U + i * 2U;
                text[i] = static_cast<wchar_t>(
                    static_cast<unsigned int>(bytes[offset]) |
                    (static_cast<unsigned int>(bytes[offset + 1U]) << 8U));
            }
            return text;
        }

        // UTF-16 big-endian BOM.
        if (bytes.size() >= 2U && bytes[0] == 0xFEU && bytes[1] == 0xFFU)
        {
            const std::size_t characterCount = (bytes.size() - 2U) / 2U;
            std::wstring text(characterCount, L'\0');
            for (std::size_t i = 0; i < characterCount; ++i)
            {
                const std::size_t offset = 2U + i * 2U;
                text[i] = static_cast<wchar_t>(
                    (static_cast<unsigned int>(bytes[offset]) << 8U) |
                    static_cast<unsigned int>(bytes[offset + 1U]));
            }
            return text;
        }

        std::size_t offset = 0;
        if (bytes.size() >= 3U &&
            bytes[0] == 0xEFU && bytes[1] == 0xBBU && bytes[2] == 0xBFU)
        {
            offset = 3U;
        }

        const std::string encoded(
            reinterpret_cast<const char*>(bytes.data() + offset),
            bytes.size() - offset);
        return widenUtf8(encoded);
    }

    std::wstring fileNameFromPath(const std::wstring& path)
    {
        const std::size_t separator = path.find_last_of(L"\\/");
        return separator == std::wstring::npos ? path : path.substr(separator + 1U);
    }
    void setStatus(const std::wstring& status)
    {
        if (g_statusLabel != nullptr)
            SetWindowTextW(g_statusLabel, status.c_str());
    }

    void pumpBatchUiMessages()
    {
        MSG message{};
        bool repostQuit = false;
        int quitCode = 0;

        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                repostQuit = true;
                quitCode = static_cast<int>(message.wParam);
                break;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (repostQuit)
            PostQuitMessage(quitCode);
    }

    void refreshBatchProgressDisplay()
    {
        if (g_progressBar != nullptr)
            RedrawWindow(g_progressBar, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        if (g_statusLabel != nullptr)
            RedrawWindow(g_statusLabel, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        pumpBatchUiMessages();
    }

    void setBatchUiActive(bool active)
    {
        g_isBatchDecrypting = active;

        if (g_menuBar != nullptr)
        {
            EnableMenuItem(
                g_menuBar,
                0,
                MF_BYPOSITION | (active ? MF_GRAYED : MF_ENABLED));
            if (g_mainWindow != nullptr)
                DrawMenuBar(g_mainWindow);
        }

        const BOOL enabled = active ? FALSE : TRUE;
        EnableWindow(g_inputTypeCombo, enabled);
        EnableWindow(g_outputTypeCombo, enabled);
        EnableWindow(g_keyCombo, enabled);
        EnableWindow(g_payloadKeyCombo, enabled);
        EnableWindow(g_groupCheck, enabled);
        EnableWindow(g_annotateCheck, enabled);
        EnableWindow(g_cmpOutputCheck, enabled);
        EnableWindow(g_duckStationCombineCheck, enabled);
        EnableWindow(g_duckStationCondenseCheck, enabled);
        EnableWindow(g_serialRepeaterCondenseCheck, enabled);
        EnableWindow(g_autoCheck, enabled);
        EnableWindow(g_convertButton, enabled);
        EnableWindow(g_copyButton, enabled);
        EnableWindow(g_clearButton, enabled);
        EnableWindow(g_swapButton, enabled);
        EnableWindow(g_inputEdit, enabled);
        EnableWindow(g_outputEdit, enabled);
        EnableWindow(g_splitter, enabled);

        if (g_inputEdit != nullptr)
            DragAcceptFiles(g_inputEdit, active ? FALSE : TRUE);

        if (g_progressBar != nullptr)
        {
            if (active)
            {
                SendMessageW(g_progressBar, PBM_SETRANGE32, 0, 1);
                SendMessageW(g_progressBar, PBM_SETPOS, 0, 0);
                ShowWindow(g_progressBar, SW_SHOW);
            }
            else
            {
                ShowWindow(g_progressBar, SW_HIDE);
                SendMessageW(g_progressBar, PBM_SETPOS, 0, 0);
            }
        }

        if (!active)
            updateModeUi();

        if (g_mainWindow != nullptr)
            layoutControls(g_mainWindow);

        refreshBatchProgressDisplay();
    }

    void updateBatchProgress(
        std::size_t completed,
        std::size_t total,
        const std::wstring& currentFile)
    {
        if (g_progressBar == nullptr)
            return;

        const std::size_t maximumInt = static_cast<std::size_t>(std::numeric_limits<int>::max());
        const int rangeMaximum = static_cast<int>(std::min(total, maximumInt));
        const int position = static_cast<int>(std::min(completed, maximumInt));

        SendMessageW(g_progressBar, PBM_SETRANGE32, 0, std::max(1, rangeMaximum));
        SendMessageW(g_progressBar, PBM_SETPOS, std::min(position, std::max(1, rangeMaximum)), 0);

        const std::size_t percent = total == 0U ? 0U : std::min<std::size_t>(100U, completed * 100U / total);
        std::wstring status;
        if (!currentFile.empty())
        {
            const std::size_t currentNumber = std::min(total, completed + 1U);
            status =
                L"Batch decrypting file " + std::to_wstring(currentNumber) + L" of " +
                std::to_wstring(total) + L" (" + std::to_wstring(percent) + L"%): " +
                currentFile;
        }
        else
        {
            status =
                L"Batch decrypted " + std::to_wstring(completed) + L" of " +
                std::to_wstring(total) + L" (" + std::to_wstring(percent) + L"%)";
        }

        setStatus(status);
        refreshBatchProgressDisplay();
    }

    void setFont(HWND hwnd)
    {
        if (hwnd != nullptr && g_uiFont != nullptr)
            SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    }

    HWND createControl(
        const wchar_t* className,
        const wchar_t* text,
        DWORD style,
        DWORD exStyle,
        int id,
        HWND parent)
    {
        HWND hwnd = CreateWindowExW(
            exStyle,
            className,
            text,
            WS_CHILD | WS_VISIBLE | style,
            0,
            0,
            10,
            10,
            parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr),
            nullptr);

        setFont(hwnd);
        return hwnd;
    }

    void addComboItem(HWND combo, const wchar_t* text)
    {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    }

    bool isChecked(HWND hwnd)
    {
        return SendMessageW(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }

    int comboIndex(HWND hwnd)
    {
        return static_cast<int>(SendMessageW(hwnd, CB_GETCURSEL, 0, 0));
    }

    void setChecked(HWND hwnd, bool checked)
    {
        if (hwnd != nullptr)
            SendMessageW(hwnd, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    void toggleChecked(HWND hwnd)
    {
        setChecked(hwnd, !isChecked(hwnd));
    }

    std::wstring settingsFilePath()
    {
        std::vector<wchar_t> modulePath(32768U, L'\0');
        const DWORD copied = GetModuleFileNameW(
            nullptr,
            modulePath.data(),
            static_cast<DWORD>(modulePath.size()));

        if (copied == 0U || copied >= modulePath.size())
            return L"XploderConverter.ini";

        std::filesystem::path path(std::wstring(modulePath.data(), copied));
        path.replace_filename(L"XploderConverter.ini");
        return path.wstring();
    }

    int readSettingInt(const wchar_t* key, int defaultValue)
    {
        const std::wstring path = settingsFilePath();
        return static_cast<int>(GetPrivateProfileIntW(L"Settings", key, defaultValue, path.c_str()));
    }

    void writeSettingInt(const wchar_t* key, int value)
    {
        const std::wstring path = settingsFilePath();
        const std::wstring text = std::to_wstring(value);
        WritePrivateProfileStringW(L"Settings", key, text.c_str(), path.c_str());
    }

    int clampedComboSetting(const wchar_t* key, int defaultValue, int maximumIndex)
    {
        return std::clamp(readSettingInt(key, defaultValue), 0, maximumIndex);
    }

    void loadPersistentSettings()
    {
        SendMessageW(g_inputTypeCombo, CB_SETCURSEL, clampedComboSetting(L"InputType", 1, 4), 0);
        SendMessageW(g_outputTypeCombo, CB_SETCURSEL, clampedComboSetting(L"OutputType", 2, 4), 0);
        SendMessageW(g_keyCombo, CB_SETCURSEL, clampedComboSetting(L"EncryptionKey", 1, 3), 0);
        SendMessageW(g_payloadKeyCombo, CB_SETCURSEL, clampedComboSetting(L"PayloadKey", 0, 1), 0);

        setChecked(g_groupCheck, readSettingInt(L"GroupEncrypted444", 0) != 0);
        setChecked(g_annotateCheck, readSettingInt(L"AnnotateCodeTypes", 0) != 0);
        setChecked(g_cmpOutputCheck, readSettingInt(L"CmpDbCompatible", 1) != 0);
        setChecked(g_autoCheck, readSettingInt(L"AutoConvert", 0) != 0);
        setChecked(g_duckStationCombineCheck, readSettingInt(L"DuckStationCombine80", 0) != 0);
        setChecked(g_duckStationCondenseCheck, readSettingInt(L"DuckStationCondenseD0", 0) != 0);
        setChecked(g_serialRepeaterCondenseCheck, readSettingInt(L"CondenseSerialRepeater", 0) != 0);

        g_hideConvertButton = readSettingInt(L"HideConvertButton", 0) != 0;
        g_hideCopyButton = readSettingInt(L"HideCopyOutputButton", 0) != 0;
        g_hideSwapButton = readSettingInt(L"HideOutputToInputButton", 0) != 0;
        g_hideClearButton = readSettingInt(L"HideClearButton", 0) != 0;
    }

    void savePersistentSettings()
    {
        if (g_inputTypeCombo == nullptr)
            return;

        writeSettingInt(L"InputType", comboIndex(g_inputTypeCombo));
        writeSettingInt(L"OutputType", comboIndex(g_outputTypeCombo));
        writeSettingInt(L"EncryptionKey", comboIndex(g_keyCombo));
        writeSettingInt(L"PayloadKey", comboIndex(g_payloadKeyCombo));
        writeSettingInt(L"GroupEncrypted444", isChecked(g_groupCheck) ? 1 : 0);
        writeSettingInt(L"AnnotateCodeTypes", isChecked(g_annotateCheck) ? 1 : 0);
        writeSettingInt(L"CmpDbCompatible", isChecked(g_cmpOutputCheck) ? 1 : 0);
        writeSettingInt(L"AutoConvert", isChecked(g_autoCheck) ? 1 : 0);
        writeSettingInt(L"DuckStationCombine80", isChecked(g_duckStationCombineCheck) ? 1 : 0);
        writeSettingInt(L"DuckStationCondenseD0", isChecked(g_duckStationCondenseCheck) ? 1 : 0);
        writeSettingInt(L"CondenseSerialRepeater", isChecked(g_serialRepeaterCondenseCheck) ? 1 : 0);
        writeSettingInt(L"HideConvertButton", g_hideConvertButton ? 1 : 0);
        writeSettingInt(L"HideCopyOutputButton", g_hideCopyButton ? 1 : 0);
        writeSettingInt(L"HideOutputToInputButton", g_hideSwapButton ? 1 : 0);
        writeSettingInt(L"HideClearButton", g_hideClearButton ? 1 : 0);
    }

    void checkMenuItemState(HMENU menu, UINT id, bool checked)
    {
        if (menu == nullptr)
            return;
        CheckMenuItem(menu, id, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
    }

    void clearMenu(HMENU menu)
    {
        if (menu == nullptr)
            return;
        while (GetMenuItemCount(menu) > 0)
            DeleteMenu(menu, 0, MF_BYPOSITION);
    }

    void appendDisabledMenuText(HMENU menu, const wchar_t* text)
    {
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0U, text);
    }

    void rebuildCurrentOutputMenu()
    {
        if (g_currentOutputMenu == nullptr || g_outputTypeCombo == nullptr)
            return;

        g_encryptionKeyMenu = nullptr;
        g_payloadKeyMenu = nullptr;
        g_autoConversionMenu = nullptr;
        clearMenu(g_currentOutputMenu);
        const psx_code_types::Family outputFamily = selectedFamily(g_outputTypeCombo);

        if (outputFamily == psx_code_types::Family::XploderEncrypted)
        {
            AppendMenuW(g_currentOutputMenu, MF_STRING, IdMenuGroupEncrypted, L"Group Encrypted 4-4-4");
            AppendMenuW(g_currentOutputMenu, MF_SEPARATOR, 0U, nullptr);

            g_encryptionKeyMenu = CreatePopupMenu();
            HMENU encryptionKeyMenu = g_encryptionKeyMenu;
            AppendMenuW(encryptionKeyMenu, MF_STRING, IdMenuEncryptionKey4, L"Key 4 / WHBX");
            AppendMenuW(encryptionKeyMenu, MF_STRING, IdMenuEncryptionKey5, L"Key 5 / WB123");
            AppendMenuW(encryptionKeyMenu, MF_STRING, IdMenuEncryptionKey6, L"Key 6 / AB+XOR");
            AppendMenuW(encryptionKeyMenu, MF_STRING, IdMenuEncryptionKey7, L"Key 7 / FCD!");
            AppendMenuW(g_currentOutputMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(encryptionKeyMenu), L"Encryption Key");

            g_payloadKeyMenu = CreatePopupMenu();
            HMENU payloadKeyMenu = g_payloadKeyMenu;
            AppendMenuW(payloadKeyMenu, MF_STRING, IdMenuPayloadKey6, L"Key 6");
            AppendMenuW(payloadKeyMenu, MF_STRING, IdMenuPayloadKey7, L"Key 7");
            AppendMenuW(g_currentOutputMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(payloadKeyMenu), L"Type 5 Payload Key");

            checkMenuItemState(g_currentOutputMenu, IdMenuGroupEncrypted, isChecked(g_groupCheck));
            CheckMenuRadioItem(
                encryptionKeyMenu,
                IdMenuEncryptionKey4,
                IdMenuEncryptionKey7,
                IdMenuEncryptionKey4 + static_cast<UINT>(std::clamp(comboIndex(g_keyCombo), 0, 3)),
                MF_BYCOMMAND);
            CheckMenuRadioItem(
                payloadKeyMenu,
                IdMenuPayloadKey6,
                IdMenuPayloadKey7,
                comboIndex(g_payloadKeyCombo) == 1 ? IdMenuPayloadKey7 : IdMenuPayloadKey6,
                MF_BYCOMMAND);
        }
        else if (outputFamily == psx_code_types::Family::GameSharkActionReplay ||
                 outputFamily == psx_code_types::Family::Caetla)
        {
            g_autoConversionMenu = CreatePopupMenu();
            HMENU conversionMenu = g_autoConversionMenu;
            AppendMenuW(conversionMenu, MF_STRING, IdMenuSerialRepeaterCondense, L"Condense Writes -> Type 5");
            checkMenuItemState(conversionMenu, IdMenuSerialRepeaterCondense, isChecked(g_serialRepeaterCondenseCheck));
            AppendMenuW(g_currentOutputMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(conversionMenu), L"Auto CodeType Conversion");
        }
        else if (outputFamily == psx_code_types::Family::DuckStation)
        {
            g_autoConversionMenu = CreatePopupMenu();
            HMENU conversionMenu = g_autoConversionMenu;
            AppendMenuW(conversionMenu, MF_STRING, IdMenuDuckStationCombine, L"80 + 80 -> 90");
            AppendMenuW(conversionMenu, MF_STRING, IdMenuDuckStationCondense, L"D0 / 70 -> C0 Block");
            AppendMenuW(conversionMenu, MF_STRING, IdMenuSerialRepeaterCondense, L"Condense Writes -> Type 5");
            checkMenuItemState(conversionMenu, IdMenuDuckStationCombine, isChecked(g_duckStationCombineCheck));
            checkMenuItemState(conversionMenu, IdMenuDuckStationCondense, isChecked(g_duckStationCondenseCheck));
            checkMenuItemState(conversionMenu, IdMenuSerialRepeaterCondense, isChecked(g_serialRepeaterCondenseCheck));
            AppendMenuW(g_currentOutputMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(conversionMenu), L"Auto CodeType Conversion");
        }
        else
        {
            appendDisabledMenuText(g_currentOutputMenu, L"No output-specific options");
        }
    }

    void applyButtonVisibility()
    {
        if (g_convertButton != nullptr)
            ShowWindow(g_convertButton, g_hideConvertButton ? SW_HIDE : SW_SHOW);
        if (g_copyButton != nullptr)
            ShowWindow(g_copyButton, g_hideCopyButton ? SW_HIDE : SW_SHOW);
        if (g_swapButton != nullptr)
            ShowWindow(g_swapButton, g_hideSwapButton ? SW_HIDE : SW_SHOW);
        if (g_clearButton != nullptr)
            ShowWindow(g_clearButton, g_hideClearButton ? SW_HIDE : SW_SHOW);
    }

    void updateOptionsMenu()
    {
        rebuildCurrentOutputMenu();
        refreshOptionsMenuChecksOnly();
        applyButtonVisibility();
        if (g_mainWindow != nullptr)
        {
            DrawMenuBar(g_mainWindow);
            layoutControls(g_mainWindow);
        }
    }

    bool isOptionsMenuCommand(int id)
    {
        return id == IdMenuAutoConvert || id == IdMenuAnnotateCodeTypes || id == IdMenuCmpDbCompatible ||
               id == IdMenuHideConvert || id == IdMenuHideCopyOutput ||
               id == IdMenuHideOutputToInput || id == IdMenuHideClear ||
               id == IdMenuGroupEncrypted ||
               (id >= IdMenuEncryptionKey4 && id <= IdMenuEncryptionKey7) ||
               id == IdMenuPayloadKey6 || id == IdMenuPayloadKey7 ||
               id == IdMenuDuckStationCombine || id == IdMenuDuckStationCondense ||
               id == IdMenuSerialRepeaterCondense;
    }

    void refreshOptionsMenuChecksOnly()
    {
        checkMenuItemState(g_programOptionsMenu, IdMenuAutoConvert, isChecked(g_autoCheck));
        checkMenuItemState(g_programOptionsMenu, IdMenuAnnotateCodeTypes, isChecked(g_annotateCheck));
        checkMenuItemState(g_programOptionsMenu, IdMenuCmpDbCompatible, isChecked(g_cmpOutputCheck));

        checkMenuItemState(g_hideButtonsMenu, IdMenuHideConvert, g_hideConvertButton);
        checkMenuItemState(g_hideButtonsMenu, IdMenuHideCopyOutput, g_hideCopyButton);
        checkMenuItemState(g_hideButtonsMenu, IdMenuHideOutputToInput, g_hideSwapButton);
        checkMenuItemState(g_hideButtonsMenu, IdMenuHideClear, g_hideClearButton);

        checkMenuItemState(g_currentOutputMenu, IdMenuGroupEncrypted, isChecked(g_groupCheck));
        checkMenuItemState(g_autoConversionMenu, IdMenuDuckStationCombine, isChecked(g_duckStationCombineCheck));
        checkMenuItemState(g_autoConversionMenu, IdMenuDuckStationCondense, isChecked(g_duckStationCondenseCheck));
        checkMenuItemState(g_autoConversionMenu, IdMenuSerialRepeaterCondense, isChecked(g_serialRepeaterCondenseCheck));

        if (g_encryptionKeyMenu != nullptr)
        {
            CheckMenuRadioItem(
                g_encryptionKeyMenu,
                IdMenuEncryptionKey4,
                IdMenuEncryptionKey7,
                IdMenuEncryptionKey4 + static_cast<UINT>(std::clamp(comboIndex(g_keyCombo), 0, 3)),
                MF_BYCOMMAND);
        }
        if (g_payloadKeyMenu != nullptr)
        {
            CheckMenuRadioItem(
                g_payloadKeyMenu,
                IdMenuPayloadKey6,
                IdMenuPayloadKey7,
                comboIndex(g_payloadKeyCombo) == 1 ? IdMenuPayloadKey7 : IdMenuPayloadKey6,
                MF_BYCOMMAND);
        }

    }

    bool handleOptionsMenuCommand(int id, bool preserveOpenMenus)
    {
        if (!isOptionsMenuCommand(id))
            return false;

        bool runConversion = false;
        bool layoutChanged = false;

        if (id == IdMenuAutoConvert)
        {
            toggleChecked(g_autoCheck);
            runConversion = isChecked(g_autoCheck);
        }
        else if (id == IdMenuAnnotateCodeTypes || id == IdMenuCmpDbCompatible)
        {
            toggleChecked(id == IdMenuAnnotateCodeTypes ? g_annotateCheck : g_cmpOutputCheck);
            runConversion = isChecked(g_autoCheck);
        }
        else if (id == IdMenuHideConvert || id == IdMenuHideCopyOutput ||
                 id == IdMenuHideOutputToInput || id == IdMenuHideClear)
        {
            if (id == IdMenuHideConvert)
                g_hideConvertButton = !g_hideConvertButton;
            else if (id == IdMenuHideCopyOutput)
                g_hideCopyButton = !g_hideCopyButton;
            else if (id == IdMenuHideOutputToInput)
                g_hideSwapButton = !g_hideSwapButton;
            else
                g_hideClearButton = !g_hideClearButton;
            layoutChanged = true;
        }
        else if (id == IdMenuGroupEncrypted || id == IdMenuDuckStationCombine ||
                 id == IdMenuDuckStationCondense || id == IdMenuSerialRepeaterCondense)
        {
            HWND target = g_groupCheck;
            if (id == IdMenuDuckStationCombine)
                target = g_duckStationCombineCheck;
            else if (id == IdMenuDuckStationCondense)
                target = g_duckStationCondenseCheck;
            else if (id == IdMenuSerialRepeaterCondense)
                target = g_serialRepeaterCondenseCheck;

            toggleChecked(target);
            runConversion = isChecked(g_autoCheck);
        }
        else if (id >= IdMenuEncryptionKey4 && id <= IdMenuEncryptionKey7)
        {
            SendMessageW(g_keyCombo, CB_SETCURSEL, id - IdMenuEncryptionKey4, 0);
            runConversion = isChecked(g_autoCheck);
        }
        else if (id == IdMenuPayloadKey6 || id == IdMenuPayloadKey7)
        {
            SendMessageW(g_payloadKeyCombo, CB_SETCURSEL, id == IdMenuPayloadKey7 ? 1 : 0, 0);
            runConversion = isChecked(g_autoCheck);
        }

        if (preserveOpenMenus)
            refreshOptionsMenuChecksOnly();
        else
            updateOptionsMenu();

        if (layoutChanged && g_mainWindow != nullptr)
        {
            if (preserveOpenMenus)
            {
                // Showing or hiding child controls while the native popup menu is
                // processing the same click can disturb its mouse tracking. Keep
                // the checkmark update immediate, but wait until the menu loop
                // closes before applying the button visibility/layout change.
                g_pendingButtonVisibilityUpdate = true;
            }
            else
            {
                applyButtonVisibility();
                layoutControls(g_mainWindow);
                g_pendingButtonVisibilityUpdate = false;
            }
        }

        savePersistentSettings();
        if (runConversion)
            convertNow();

        return true;
    }

    bool menuCommandFromMenuPosition(HMENU menu, int position, int& commandId)
    {
        if (menu == nullptr || position < 0)
            return false;

        MENUITEMINFOW item{};
        item.cbSize = sizeof(item);
        item.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU;
        if (GetMenuItemInfoW(menu, static_cast<UINT>(position), TRUE, &item) == FALSE)
            return false;
        if (item.hSubMenu != nullptr || (item.fType & MFT_SEPARATOR) != 0U ||
            (item.fState & (MFS_DISABLED | MFS_GRAYED)) != 0U)
            return false;
        if (!isOptionsMenuCommand(static_cast<int>(item.wID)))
            return false;

        commandId = static_cast<int>(item.wID);
        return true;
    }

    bool menuCommandAtPoint(POINT point, int& commandId)
    {
        // WM_MENUSELECT already tells us which exact popup and row Windows has
        // highlighted. Prefer that active menu instead of searching every menu
        // handle, because inactive popup menus can retain stale rectangles which
        // overlap the left/checkmark side of another submenu.
        if (g_selectedMenu != nullptr)
        {
            const int position = MenuItemFromPoint(g_mainWindow, g_selectedMenu, point);
            if (menuCommandFromMenuPosition(g_selectedMenu, position, commandId))
                return true;

            const int selectedId = selectedKeyboardMenuCommand();
            if (selectedId != 0)
            {
                commandId = selectedId;
                return true;
            }
        }

        return false;
    }

    int selectedKeyboardMenuCommand()
    {
        if (g_selectedMenu == nullptr ||
            (g_selectedMenuFlags & (MF_POPUP | MF_SEPARATOR | MF_DISABLED | MF_GRAYED)) != 0U)
            return 0;

        const int id = static_cast<int>(g_selectedMenuItem);
        return isOptionsMenuCommand(id) ? id : 0;
    }

    BOOL CALLBACK redrawPopupMenuWindow(HWND hwnd, LPARAM)
    {
        wchar_t className[32]{};
        constexpr int classNameLength = static_cast<int>(sizeof(className) / sizeof(className[0]));
        if (GetClassNameW(hwnd, className, classNameLength) <= 0 ||
            lstrcmpW(className, L"#32768") != 0 || IsWindowVisible(hwnd) == FALSE)
        {
            return TRUE;
        }

        // Native popup menus cache the highlighted row until another mouse move.
        // Force every open popup in the current menu chain to repaint now so the
        // new check/radio state is visible on the same click that changed it.
        RedrawWindow(
            hwnd,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
        return TRUE;
    }

    void redrawOpenPopupMenusImmediately()
    {
        EnumThreadWindows(GetCurrentThreadId(), redrawPopupMenuWindow, 0);
    }

    LRESULT CALLBACK optionsMenuMessageFilter(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == MSGF_MENU && lParam != 0)
        {
            MSG* message = reinterpret_cast<MSG*>(lParam);
            int commandId = 0;

            if (message->message == WM_LBUTTONUP && menuCommandAtPoint(message->pt, commandId))
            {
                if (handleOptionsMenuCommand(commandId, true))
                {
                    redrawOpenPopupMenusImmediately();
                    return 1;
                }
            }
            else if (message->message == WM_KEYDOWN &&
                     (message->wParam == VK_RETURN || message->wParam == VK_SPACE))
            {
                commandId = selectedKeyboardMenuCommand();
                if (commandId != 0 && handleOptionsMenuCommand(commandId, true))
                {
                    redrawOpenPopupMenusImmediately();
                    return 1;
                }
            }
        }

        return CallNextHookEx(g_optionsMenuMessageHook, code, wParam, lParam);
    }

    void createOptionsMenu(HWND hwnd)
    {
        g_menuBar = CreateMenu();
        g_optionsMenu = CreatePopupMenu();
        g_programOptionsMenu = CreatePopupMenu();
        g_hideButtonsMenu = CreatePopupMenu();
        g_currentOutputMenu = CreatePopupMenu();

        AppendMenuW(g_programOptionsMenu, MF_STRING, IdMenuAutoConvert, L"Auto Convert");
        AppendMenuW(g_programOptionsMenu, MF_STRING, IdMenuAnnotateCodeTypes, L"Annotate Code Types (Xploder)");
        AppendMenuW(g_programOptionsMenu, MF_STRING, IdMenuCmpDbCompatible, L"CMP DB Compatible Output");

        AppendMenuW(g_hideButtonsMenu, MF_STRING, IdMenuHideConvert, L"Hide Convert");
        AppendMenuW(g_hideButtonsMenu, MF_STRING, IdMenuHideCopyOutput, L"Hide Copy Output");
        AppendMenuW(g_hideButtonsMenu, MF_STRING, IdMenuHideOutputToInput, L"Hide Output -> Input");
        AppendMenuW(g_hideButtonsMenu, MF_STRING, IdMenuHideClear, L"Hide Clear");

        AppendMenuW(g_optionsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(g_programOptionsMenu), L"Program Options");
        AppendMenuW(g_optionsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(g_hideButtonsMenu), L"Hide Buttons");
        AppendMenuW(g_optionsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(g_currentOutputMenu), L"Current Output");
        AppendMenuW(g_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(g_optionsMenu), L"Options");
        SetMenu(hwnd, g_menuBar);
    }

    bool loadDroppedTextFile(const std::wstring& path)
    {
        std::vector<std::uint8_t> bytes;
        std::wstring error;
        if (!readDroppedFileBytes(path, bytes, error))
        {
            setStatus(std::wstring(L"Could not load dropped file: ") + error);
            MessageBeep(MB_ICONWARNING);
            return false;
        }

        std::wstring text = decodeDroppedText(bytes);
        while (!text.empty() && text.back() == L'\0')
            text.pop_back();

        if (text.find(L'\0') != std::wstring::npos)
        {
            setStatus(L"The dropped file contains binary data and was not loaded.");
            MessageBeep(MB_ICONWARNING);
            return false;
        }

        g_isLoadingInput = true;
        setEditText(g_inputEdit, text);
        g_isLoadingInput = false;

        SendMessageW(g_inputEdit, EM_SETSEL, 0, 0);
        SendMessageW(g_inputEdit, EM_SCROLLCARET, 0, 0);
        SetFocus(g_inputEdit);

        setStatus(std::wstring(L"Loaded ") + fileNameFromPath(path) + L" into Input.");
        if (isChecked(g_autoCheck))
            convertNow();

        return true;
    }

    LRESULT CALLBACK inputEditSubclassProc(
        HWND hwnd,
        UINT msg,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR)
    {
        if (msg == WM_PASTE)
        {
            if (pasteNormalizedClipboardText(hwnd))
                return 0;
        }

        if (handleEditShortcut(hwnd, msg, wParam, true))
            return 0;

        if (isHandledEditShortcutCharacter(msg, wParam))
            return 0;

        if (msg == WM_DROPFILES)
        {
            HDROP drop = reinterpret_cast<HDROP>(wParam);
            if (g_isBatchDecrypting)
            {
                DragFinish(drop);
                setStatus(L"A folder batch is already running.");
                MessageBeep(MB_ICONWARNING);
                return 0;
            }
            const UINT fileCount = DragQueryFileW(drop, 0xFFFFFFFFU, nullptr, 0);

            if (fileCount != 1U)
            {
                setStatus(L"Drop one text file or one folder at a time onto the Input pane.");
                MessageBeep(MB_ICONWARNING);
                DragFinish(drop);
                return 0;
            }

            const UINT pathLength = DragQueryFileW(drop, 0U, nullptr, 0);
            std::wstring path(static_cast<std::size_t>(pathLength) + 1U, L'\0');
            DragQueryFileW(drop, 0U, path.data(), pathLength + 1U);
            path.resize(pathLength);

            DragFinish(drop);

            const DWORD attributes = GetFileAttributesW(path.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES &&
                (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                batchDecryptFolder(path);
            }
            else
            {
                loadDroppedTextFile(path);
            }
            return 0;
        }

        if (msg == WM_NCDESTROY)
        {
            DragAcceptFiles(hwnd, FALSE);
            RemoveWindowSubclass(hwnd, inputEditSubclassProc, subclassId);
        }

        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    LRESULT CALLBACK outputEditSubclassProc(
        HWND hwnd,
        UINT msg,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR)
    {
        if (handleEditShortcut(hwnd, msg, wParam, false))
            return 0;

        if (isHandledEditShortcutCharacter(msg, wParam))
            return 0;

        if (msg == WM_NCDESTROY)
            RemoveWindowSubclass(hwnd, outputEditSubclassProc, subclassId);

        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    xploder_psx::Key selectedEncryptionKey()
    {
        switch (comboIndex(g_keyCombo))
        {
            case 0: return xploder_psx::Key::Key4;
            case 1: return xploder_psx::Key::Key5;
            case 2: return xploder_psx::Key::Key6;
            case 3: return xploder_psx::Key::Key7;
            default: return xploder_psx::Key::Key5;
        }
    }

    psx_code_types::Family selectedFamily(HWND combo)
    {
        switch (comboIndex(combo))
        {
            case 0: return psx_code_types::Family::GameSharkActionReplay;
            case 1: return psx_code_types::Family::XploderEncrypted;
            case 2: return psx_code_types::Family::XploderRaw;
            case 3: return psx_code_types::Family::DuckStation;
            case 4: return psx_code_types::Family::Caetla;
            default: return psx_code_types::Family::XploderRaw;
        }
    }

    xploder_converter::Options selectedOptions()
    {
        xploder_converter::Options options;
        // Mode is selected internally by the window conversion coordinator.
        // Folder drop explicitly forces Decrypt and never reads the family selectors.
        options.mode = xploder_converter::Mode::Decrypt;
        options.encryptionKey = selectedEncryptionKey();
        options.massWritePayloadKey = comboIndex(g_payloadKeyCombo) == 1 ? 7 : 6;
        options.groupEncryptedOutput = isChecked(g_groupCheck);
        options.annotateCodeTypes = isChecked(g_annotateCheck);
        options.outputCmpDbCompatible = isChecked(g_cmpOutputCheck);
        return options;
    }

    psx_code_types::WindowConversionOptions selectedWindowOptions()
    {
        psx_code_types::WindowConversionOptions options;
        options.inputFamily = selectedFamily(g_inputTypeCombo);
        options.outputFamily = selectedFamily(g_outputTypeCombo);
        options.combineDuckStation16BitWrites = isChecked(g_duckStationCombineCheck);
        options.condenseDuckStationActivators = isChecked(g_duckStationCondenseCheck);
        options.condenseBasicSerialRepeaters = isChecked(g_serialRepeaterCondenseCheck);
        options.xploderOptions = selectedOptions();
        return options;
    }

    bool equalsIgnoreCase(const std::wstring& left, const std::wstring& right)
    {
        if (left.size() != right.size())
            return false;

        for (std::size_t i = 0; i < left.size(); ++i)
        {
            if (std::towlower(left[i]) != std::towlower(right[i]))
                return false;
        }
        return true;
    }

    bool isTextFilePath(const std::filesystem::path& path)
    {
        return equalsIgnoreCase(path.extension().wstring(), L".txt");
    }

    std::string useWindowsLineEndings(const std::string& text)
    {
        std::string output;
        output.reserve(text.size() + text.size() / 16U + 16U);

        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const char c = text[i];
            if (c == '\r')
            {
                output += "\r\n";
                if (i + 1U < text.size() && text[i + 1U] == '\n')
                    ++i;
            }
            else if (c == '\n')
            {
                output += "\r\n";
            }
            else
            {
                output.push_back(c);
            }
        }
        return output;
    }

    bool writeUtf8TextFile(
        const std::filesystem::path& path,
        const std::string& text,
        std::wstring& error)
    {
        error.clear();
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            error = L"Could not create the output file.";
            return false;
        }

        const std::string windowsText = useWindowsLineEndings(text);
        file.write(windowsText.data(), static_cast<std::streamsize>(windowsText.size()));
        if (!file)
        {
            error = L"Could not finish writing the output file.";
            return false;
        }
        return true;
    }

    class BatchUiScope
    {
    public:
        BatchUiScope()
        {
            setBatchUiActive(true);
        }

        ~BatchUiScope()
        {
            finish();
        }

        void finish()
        {
            if (active_)
            {
                active_ = false;
                setBatchUiActive(false);
            }
        }

        BatchUiScope(const BatchUiScope&) = delete;
        BatchUiScope& operator=(const BatchUiScope&) = delete;

    private:
        bool active_ = true;
    };

    bool batchDecryptFolder(const std::wstring& folderPath)
    {
        namespace fs = std::filesystem;

        if (g_isBatchDecrypting)
            return false;

        BatchUiScope batchUi;
        setStatus(L"Scanning dropped folder for .txt files...");
        refreshBatchProgressDisplay();

        const fs::path sourceRoot(folderPath);
        const fs::path outputRoot = sourceRoot / L"Decrypted";
        std::vector<fs::path> inputFiles;
        std::error_code errorCode;

        fs::recursive_directory_iterator iterator(
            sourceRoot,
            fs::directory_options::skip_permission_denied,
            errorCode);
        const fs::recursive_directory_iterator end;

        if (errorCode)
        {
            setStatus(L"Could not scan the dropped folder.");
            MessageBeep(MB_ICONWARNING);
            return false;
        }

        while (iterator != end)
        {
            const fs::directory_entry entry = *iterator;
            std::error_code entryError;

            if (entry.is_directory(entryError))
            {
                // Never process a previous batch's output as new input.
                if (equalsIgnoreCase(entry.path().filename().wstring(), L"Decrypted"))
                    iterator.disable_recursion_pending();
            }
            else if (!entryError && entry.is_regular_file(entryError) &&
                     !entryError && isTextFilePath(entry.path()))
            {
                inputFiles.push_back(entry.path());
            }

            iterator.increment(errorCode);
            if (errorCode)
                errorCode.clear();
        }

        std::sort(inputFiles.begin(), inputFiles.end());
        if (inputFiles.empty())
        {
            setStatus(L"The dropped folder does not contain any .txt files.");
            MessageBeep(MB_ICONWARNING);
            return false;
        }

        fs::create_directories(outputRoot, errorCode);
        if (errorCode)
        {
            setStatus(L"Could not create the Decrypted output folder.");
            MessageBeep(MB_ICONWARNING);
            return false;
        }

        xploder_converter::Options options = selectedOptions();
        options.mode = xploder_converter::Mode::Decrypt;
        options.groupEncryptedOutput = false;

        std::size_t convertedCount = 0;
        std::size_t failedCount = 0;
        std::wstring firstFailure;

        SendMessageW(
            g_progressBar,
            PBM_SETRANGE32,
            0,
            static_cast<LPARAM>(std::min<std::size_t>(
                inputFiles.size(),
                static_cast<std::size_t>(std::numeric_limits<int>::max()))));
        SendMessageW(g_progressBar, PBM_SETPOS, 0, 0);

        for (std::size_t fileIndex = 0; fileIndex < inputFiles.size(); ++fileIndex)
        {
            const fs::path& inputPath = inputFiles[fileIndex];
            updateBatchProgress(fileIndex, inputFiles.size(), inputPath.filename().wstring());

            std::vector<std::uint8_t> bytes;
            std::wstring fileError;
            if (!readDroppedFileBytes(inputPath.wstring(), bytes, fileError))
            {
                ++failedCount;
                if (firstFailure.empty())
                    firstFailure = inputPath.filename().wstring() + L": " + fileError;
                updateBatchProgress(fileIndex + 1U, inputFiles.size(), L"");
                continue;
            }

            std::wstring decoded = decodeDroppedText(bytes);
            while (!decoded.empty() && decoded.back() == L'\0')
                decoded.pop_back();
            if (decoded.find(L'\0') != std::wstring::npos)
            {
                ++failedCount;
                if (firstFailure.empty())
                    firstFailure = inputPath.filename().wstring() + L": binary data detected.";
                updateBatchProgress(fileIndex + 1U, inputFiles.size(), L"");
                continue;
            }

            try
            {
                const std::string sourceUtf8 = narrowUtf8(decoded);
                const std::string normalized =
                    xploder_converter::normalizeBatchDecryptInput(
                        sourceUtf8, options.outputCmpDbCompatible);
                const std::string decrypted =
                    xploder_converter::convertText(normalized, options);

                fs::path relativePath = inputPath.lexically_relative(sourceRoot);
                if (relativePath.empty() || relativePath.native().find(L"..") == 0U)
                    relativePath = inputPath.filename();

                const fs::path outputPath = outputRoot / relativePath;
                fs::create_directories(outputPath.parent_path(), errorCode);
                if (errorCode)
                {
                    ++failedCount;
                    if (firstFailure.empty())
                        firstFailure = outputPath.filename().wstring() + L": could not create its output folder.";
                    errorCode.clear();
                    updateBatchProgress(fileIndex + 1U, inputFiles.size(), L"");
                continue;
                }

                if (!writeUtf8TextFile(outputPath, decrypted, fileError))
                {
                    ++failedCount;
                    if (firstFailure.empty())
                        firstFailure = outputPath.filename().wstring() + L": " + fileError;
                    updateBatchProgress(fileIndex + 1U, inputFiles.size(), L"");
                continue;
                }

                ++convertedCount;
            }
            catch (const std::exception& ex)
            {
                ++failedCount;
                if (firstFailure.empty())
                    firstFailure = inputPath.filename().wstring() + L": " + widenUtf8(ex.what());
            }
            catch (...)
            {
                ++failedCount;
                if (firstFailure.empty())
                    firstFailure = inputPath.filename().wstring() + L": unknown conversion error.";
            }

            updateBatchProgress(fileIndex + 1U, inputFiles.size(), L"");
        }

        std::wstring summary =
            L"Batch decrypted " + std::to_wstring(convertedCount) +
            L" text file(s) into:\n" + outputRoot.wstring();
        if (failedCount != 0U)
        {
            summary += L"\n\nFailed: " + std::to_wstring(failedCount);
            if (!firstFailure.empty())
                summary += L"\nFirst error: " + firstFailure;
        }

        batchUi.finish();
        setStatus(
            L"Batch complete: " + std::to_wstring(convertedCount) +
            L" decrypted, " + std::to_wstring(failedCount) + L" failed.");
        MessageBoxW(
            g_mainWindow,
            summary.c_str(),
            L"Folder Batch Decrypt",
            failedCount == 0U ? MB_OK | MB_ICONINFORMATION : MB_OK | MB_ICONWARNING);
        return convertedCount != 0U;
    }

    void updateModeUi()
    {
        // Output-specific settings are exposed through Options > Current Output.
        // The legacy child controls remain as hidden state holders so the
        // conversion core and folder batch path keep using the same options.
        ShowWindow(g_keyCombo, SW_HIDE);
        ShowWindow(g_payloadKeyCombo, SW_HIDE);
        ShowWindow(g_groupCheck, SW_HIDE);
        ShowWindow(g_annotateCheck, SW_HIDE);
        ShowWindow(g_cmpOutputCheck, SW_HIDE);
        ShowWindow(g_autoCheck, SW_HIDE);
        ShowWindow(g_duckStationCombineCheck, SW_HIDE);
        ShowWindow(g_duckStationCondenseCheck, SW_HIDE);
        ShowWindow(g_serialRepeaterCondenseCheck, SW_HIDE);

        updateOptionsMenu();
    }

    void convertNow()
    {
        if (g_isConverting)
            return;

        g_isConverting = true;
        try
        {
            const std::wstring inputWide = getWindowText(g_inputEdit);
            const std::string inputUtf8 = narrowUtf8(inputWide);
            const psx_code_types::WindowConversionOptions options = selectedWindowOptions();
            const std::string outputUtf8 = psx_code_types::convertWindowText(inputUtf8, options);
            setEditText(g_outputEdit, widenUtf8(outputUtf8));

            const std::wstring inputName = widenUtf8(psx_code_types::familyName(options.inputFamily));
            const std::wstring outputName = widenUtf8(psx_code_types::familyName(options.outputFamily));
            setStatus(L"Done: " + inputName + L" -> " + outputName + L" conversion complete.");
        }
        catch (const std::exception& ex)
        {
            setEditText(g_outputEdit, widenUtf8(std::string("ERROR: ") + ex.what()));
            setStatus(L"Conversion failed.");
        }
        catch (...)
        {
            setEditText(g_outputEdit, L"ERROR: Unknown conversion failure.");
            setStatus(L"Conversion failed.");
        }
        g_isConverting = false;
    }

    void copyOutputToClipboard()
    {
        const std::wstring text = getWindowText(g_outputEdit);
        if (text.empty())
        {
            setStatus(L"Output is empty; nothing copied.");
            return;
        }

        if (!OpenClipboard(g_mainWindow))
        {
            setStatus(L"Could not open clipboard.");
            return;
        }

        EmptyClipboard();
        const SIZE_T bytes = (text.size() + 1U) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (memory != nullptr)
        {
            void* dest = GlobalLock(memory);
            if (dest != nullptr)
            {
                CopyMemory(dest, text.c_str(), bytes);
                GlobalUnlock(memory);
                SetClipboardData(CF_UNICODETEXT, memory);
                memory = nullptr; // Clipboard owns it now.
                setStatus(L"Output copied to clipboard.");
            }
        }

        if (memory != nullptr)
            GlobalFree(memory);
        CloseClipboard();
    }

    void swapOutputToInput()
    {
        const std::wstring output = getWindowText(g_outputEdit);
        setEditText(g_inputEdit, output);
        const int outputTypeIndex = comboIndex(g_outputTypeCombo);
        if (outputTypeIndex >= 0)
            SendMessageW(g_inputTypeCombo, CB_SETCURSEL, static_cast<WPARAM>(outputTypeIndex), 0);
        updateModeUi();
        savePersistentSettings();
        setStatus(L"Output moved to input; Input Type now matches the previous Output Type.");
        if (isChecked(g_autoCheck))
            convertNow();
    }

    void clearText()
    {
        setWindowText(g_inputEdit, L"");
        setWindowText(g_outputEdit, L"");
        setStatus(L"Cleared.");
    }

    struct PaneLayout
    {
        int leftWidth = 0;
        int rightWidth = 0;
        int splitterX = 0;
        int rightX = 0;
        int labelY = 0;
        int labelHeight = 0;
        int editorY = 0;
        int editorHeight = 0;
    };

    PaneLayout calculatePaneLayout(HWND hwnd)
    {
        RECT rc{};
        GetClientRect(hwnd, &rc);

        const int width = rc.right - rc.left;
        const int height = rc.bottom - rc.top;
        const int margin = LayoutMargin;
        const int top = 10;
        const int controlsH = 86;
        const int labelH = 20;
        const int statusH = 24;
        const int progressH = 18;
        const int progressGap = 6;
        const bool progressVisible = g_progressBar != nullptr && IsWindowVisible(g_progressBar) != FALSE;
        const int batchAreaH = progressVisible ? progressH + progressGap : 0;
        const int editorTop = top + controlsH + labelH + 4;
        const int editorBottom = height - statusH - batchAreaH - margin;
        const int editorH = std::max(40, editorBottom - editorTop);

        const int availablePaneWidth = std::max(0, width - margin * 2 - SplitterWidth);
        const int effectiveMinimum = std::min(MinimumPaneWidth, availablePaneWidth / 2);
        int leftPaneWidth = static_cast<int>(availablePaneWidth * g_splitRatio);
        leftPaneWidth = std::clamp(
            leftPaneWidth,
            effectiveMinimum,
            std::max(effectiveMinimum, availablePaneWidth - effectiveMinimum));

        PaneLayout layout;
        layout.leftWidth = leftPaneWidth;
        layout.rightWidth = std::max(0, availablePaneWidth - leftPaneWidth);
        layout.splitterX = margin + leftPaneWidth;
        layout.rightX = layout.splitterX + SplitterWidth;
        layout.labelY = top + controlsH;
        layout.labelHeight = labelH;
        layout.editorY = editorTop;
        layout.editorHeight = editorH;
        return layout;
    }

    bool deferControl(HDWP& batch, HWND control, int x, int y, int width, int height, UINT flags)
    {
        if (batch == nullptr || control == nullptr)
            return false;

        batch = DeferWindowPos(batch, control, nullptr, x, y, width, height, flags);
        return batch != nullptr;
    }

    void layoutPaneControls(HWND hwnd, bool repaintImmediately)
    {
        if (hwnd == nullptr)
            return;

        const PaneLayout layout = calculatePaneLayout(hwnd);
        constexpr UINT flags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER;

        // Defer the five pane-window moves so Windows commits them together.
        // Do not use SWP_NOREDRAW here: it leaves stale pixels behind when an
        // EDIT control changes size or moves across another child window.
        HDWP batch = BeginDeferWindowPos(5);
        bool batched = batch != nullptr;
        if (batched)
            batched = deferControl(batch, g_inputLabel, LayoutMargin, layout.labelY, layout.leftWidth, layout.labelHeight, flags);
        if (batched)
            batched = deferControl(batch, g_outputLabel, layout.rightX, layout.labelY, layout.rightWidth, layout.labelHeight, flags);
        if (batched)
            batched = deferControl(batch, g_inputEdit, LayoutMargin, layout.editorY, layout.leftWidth, layout.editorHeight, flags);
        if (batched)
            batched = deferControl(batch, g_splitter, layout.splitterX, layout.labelY, SplitterWidth, layout.labelHeight + 4 + layout.editorHeight, flags);
        if (batched)
            batched = deferControl(batch, g_outputEdit, layout.rightX, layout.editorY, layout.rightWidth, layout.editorHeight, flags);

        if (batched)
        {
            EndDeferWindowPos(batch);
        }
        else
        {
            SetWindowPos(g_inputLabel, nullptr, LayoutMargin, layout.labelY, layout.leftWidth, layout.labelHeight, flags);
            SetWindowPos(g_outputLabel, nullptr, layout.rightX, layout.labelY, layout.rightWidth, layout.labelHeight, flags);
            SetWindowPos(g_inputEdit, nullptr, LayoutMargin, layout.editorY, layout.leftWidth, layout.editorHeight, flags);
            SetWindowPos(g_splitter, nullptr, layout.splitterX, layout.labelY, SplitterWidth, layout.labelHeight + 4 + layout.editorHeight, flags);
            SetWindowPos(g_outputEdit, nullptr, layout.rightX, layout.editorY, layout.rightWidth, layout.editorHeight, flags);
        }

        // A committed layout happens only after drag release or a window resize.
        // Force one clean paint then; mouse movement itself only moves the thin
        // splitter preview and therefore does not repaint both large editors.
        if (repaintImmediately)
        {
            RedrawWindow(hwnd, nullptr, nullptr,
                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
    }

    void updateSplitterFromCursor(HWND splitter, bool commitLayout)
    {
        HWND parent = GetParent(splitter);
        if (parent == nullptr)
            return;

        RECT rc{};
        GetClientRect(parent, &rc);
        const int clientWidth = rc.right - rc.left;
        const int availableWidth = clientWidth - LayoutMargin * 2 - SplitterWidth;
        if (availableWidth <= 0)
            return;

        POINT cursor{};
        GetCursorPos(&cursor);
        ScreenToClient(parent, &cursor);

        const int effectiveMinimum = std::min(MinimumPaneWidth, availableWidth / 2);
        const int requestedLeftWidth = cursor.x - LayoutMargin - SplitterWidth / 2;
        const int leftWidth = std::clamp(
            requestedLeftWidth,
            effectiveMinimum,
            availableWidth - effectiveMinimum);

        const double newRatio = static_cast<double>(leftWidth) / static_cast<double>(availableWidth);
        const int previewX = LayoutMargin + leftWidth;

        g_splitRatio = newRatio;

        if (commitLayout)
        {
            layoutPaneControls(parent, true);
            return;
        }

        // During the drag, move only the narrow splitter as a preview.  The
        // expensive Input/Output EDIT controls are resized once on release.
        // Keeping the splitter on top makes the preview visible over either pane.
        RECT splitterRect{};
        GetWindowRect(splitter, &splitterRect);
        const int splitterHeight = splitterRect.bottom - splitterRect.top;
        const PaneLayout layout = calculatePaneLayout(parent);
        SetWindowPos(
            splitter,
            HWND_TOP,
            previewX,
            layout.labelY,
            SplitterWidth,
            splitterHeight,
            SWP_NOACTIVATE | SWP_NOCOPYBITS);
        UpdateWindow(splitter);
    }

    LRESULT CALLBACK splitterProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
            case WM_SETCURSOR:
                SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                return TRUE;

            case WM_LBUTTONDOWN:
                g_isDraggingSplitter = true;
                SetCapture(hwnd);
                updateSplitterFromCursor(hwnd, false);
                return 0;

            case WM_MOUSEMOVE:
                if (g_isDraggingSplitter && GetCapture() == hwnd)
                    updateSplitterFromCursor(hwnd, false);
                return 0;

            case WM_LBUTTONUP:
                if (g_isDraggingSplitter)
                {
                    updateSplitterFromCursor(hwnd, true);
                    g_isDraggingSplitter = false;
                    if (GetCapture() == hwnd)
                        ReleaseCapture();
                }
                return 0;

            case WM_CAPTURECHANGED:
                if (g_isDraggingSplitter)
                {
                    g_isDraggingSplitter = false;
                    HWND parent = GetParent(hwnd);
                    if (parent != nullptr)
                        layoutPaneControls(parent, true);
                }
                return 0;

            case WM_PAINT:
            {
                PAINTSTRUCT ps{};
                HDC dc = BeginPaint(hwnd, &ps);
                RECT rc{};
                GetClientRect(hwnd, &rc);
                FillRect(dc, &rc, GetSysColorBrush(COLOR_3DFACE));

                const int centerX = (rc.right - rc.left) / 2;
                RECT grip{centerX, rc.top + 6, centerX + 1, std::max(rc.top + 6, rc.bottom - 6)};
                FillRect(dc, &grip, GetSysColorBrush(COLOR_3DSHADOW));
                EndPaint(hwnd, &ps);
                return 0;
            }
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void layoutControls(HWND hwnd)
    {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const int width = rc.right - rc.left;
        const int height = rc.bottom - rc.top;
        const int margin = LayoutMargin;
        const int gap = 10;
        const int top = 10;
        const int statusH = 24;
        const int progressH = 18;
        const int progressGap = 6;

        constexpr int familyComboWidth = 240;
        int x = margin;
        int y = top;

        MoveWindow(g_inputTypeCombo, x, y + 4, familyComboWidth, 220, TRUE);
        x += familyComboWidth + gap;
        MoveWindow(g_outputTypeCombo, x, y + 4, familyComboWidth, 220, TRUE);

        x = margin;
        y = top + 42;
        const auto placeVisibleButton = [&](HWND button, int buttonWidth)
        {
            if (button != nullptr && IsWindowVisible(button) != FALSE)
            {
                MoveWindow(button, x, y, buttonWidth, 28, TRUE);
                x += buttonWidth + gap;
            }
        };

        placeVisibleButton(g_convertButton, 90);
        placeVisibleButton(g_copyButton, 105);
        placeVisibleButton(g_swapButton, 115);
        placeVisibleButton(g_clearButton, 75);

        layoutPaneControls(hwnd, true);
        if (g_progressBar != nullptr && IsWindowVisible(g_progressBar) != FALSE)
        {
            MoveWindow(
                g_progressBar,
                margin,
                height - statusH - progressGap - progressH,
                std::max(10, width - margin * 2),
                progressH,
                TRUE);
        }
        MoveWindow(g_statusLabel, margin, height - statusH, width - margin * 2, statusH, TRUE);
    }

    void createUi(HWND hwnd)
    {
        INITCOMMONCONTROLSEX icc{};
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
        InitCommonControlsEx(&icc);

        g_uiFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        createOptionsMenu(hwnd);

        g_inputTypeCombo = createControl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, IdInputTypeCombo, hwnd);
        addComboItem(g_inputTypeCombo, L"Input: GameShark / Action Replay");
        addComboItem(g_inputTypeCombo, L"Input: Xploder Encrypted");
        addComboItem(g_inputTypeCombo, L"Input: Xploder RAW");
        addComboItem(g_inputTypeCombo, L"Input: DuckStation");
        addComboItem(g_inputTypeCombo, L"Input: Caetla");
        SendMessageW(g_inputTypeCombo, CB_SETCURSEL, 1, 0);

        g_outputTypeCombo = createControl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, IdOutputTypeCombo, hwnd);
        addComboItem(g_outputTypeCombo, L"Output: GameShark / Action Replay");
        addComboItem(g_outputTypeCombo, L"Output: Xploder Encrypted");
        addComboItem(g_outputTypeCombo, L"Output: Xploder RAW");
        addComboItem(g_outputTypeCombo, L"Output: DuckStation");
        addComboItem(g_outputTypeCombo, L"Output: Caetla");
        SendMessageW(g_outputTypeCombo, CB_SETCURSEL, 2, 0);

        g_duckStationCombineCheck = createControl(
            L"BUTTON",
            L"Combine 80 + 80 -> 90",
            BS_AUTOCHECKBOX | WS_TABSTOP,
            0,
            IdDuckStationCombineCheck,
            hwnd);
        SendMessageW(g_duckStationCombineCheck, BM_SETCHECK, BST_UNCHECKED, 0);
        ShowWindow(g_duckStationCombineCheck, SW_HIDE);

        g_duckStationCondenseCheck = createControl(
            L"BUTTON",
            L"Condense D0 -> C0 Block",
            BS_AUTOCHECKBOX | WS_TABSTOP,
            0,
            IdDuckStationCondenseCheck,
            hwnd);
        SendMessageW(g_duckStationCondenseCheck, BM_SETCHECK, BST_UNCHECKED, 0);
        ShowWindow(g_duckStationCondenseCheck, SW_HIDE);

        g_serialRepeaterCondenseCheck = createControl(
            L"BUTTON",
            L"Condense Writes -> Type 5",
            BS_AUTOCHECKBOX | WS_TABSTOP,
            0,
            IdSerialRepeaterCondenseCheck,
            hwnd);
        SendMessageW(g_serialRepeaterCondenseCheck, BM_SETCHECK, BST_UNCHECKED, 0);
        ShowWindow(g_serialRepeaterCondenseCheck, SW_HIDE);

        g_keyCombo = createControl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, IdKeyCombo, hwnd);
        addComboItem(g_keyCombo, L"Encrypt Key 4 / WHBX style");
        addComboItem(g_keyCombo, L"Encrypt Key 5 / WB123 style");
        addComboItem(g_keyCombo, L"Encrypt Key 6 / AB+XOR style");
        addComboItem(g_keyCombo, L"Encrypt Key 7 / FCD! style");
        SendMessageW(g_keyCombo, CB_SETCURSEL, 1, 0);

        g_payloadKeyCombo = createControl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, IdPayloadKeyCombo, hwnd);
        addComboItem(g_payloadKeyCombo, L"Type 5 payload key 6");
        addComboItem(g_payloadKeyCombo, L"Type 5 payload key 7");
        SendMessageW(g_payloadKeyCombo, CB_SETCURSEL, 0, 0);

        g_convertButton = createControl(L"BUTTON", L"Convert", BS_PUSHBUTTON | WS_TABSTOP, 0, IdConvertButton, hwnd);
        g_copyButton = createControl(L"BUTTON", L"Copy Output", BS_PUSHBUTTON | WS_TABSTOP, 0, IdCopyButton, hwnd);
        g_swapButton = createControl(L"BUTTON", L"Output -> Input", BS_PUSHBUTTON | WS_TABSTOP, 0, IdSwapButton, hwnd);
        g_clearButton = createControl(L"BUTTON", L"Clear", BS_PUSHBUTTON | WS_TABSTOP, 0, IdClearButton, hwnd);

        g_groupCheck = createControl(L"BUTTON", L"Group encrypted 4-4-4", BS_AUTOCHECKBOX | WS_TABSTOP, 0, IdGroupCheck, hwnd);
        g_annotateCheck = createControl(L"BUTTON", L"Annotate code types", BS_AUTOCHECKBOX | WS_TABSTOP, 0, IdAnnotateCheck, hwnd);
        g_cmpOutputCheck = createControl(L"BUTTON", L"Output CMP DB Compatible", BS_AUTOCHECKBOX | WS_TABSTOP, 0, IdCmpOutputCheck, hwnd);
        SendMessageW(g_cmpOutputCheck, BM_SETCHECK, BST_CHECKED, 0);
        g_autoCheck = createControl(L"BUTTON", L"Auto Convert", BS_AUTOCHECKBOX | WS_TABSTOP, 0, IdAutoCheck, hwnd);

        g_inputLabel = createControl(L"STATIC", L"Input (file/folder drop; type selectors are editor-only)", 0, 0, 0, hwnd);
        g_outputLabel = createControl(L"STATIC", L"Output", 0, 0, 0, hwnd);
        g_splitter = createControl(SplitterClassName, L"", 0, 0, IdSplitter, hwnd);

        const DWORD editStyle = WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN;
        g_inputEdit = createControl(L"EDIT", L"", editStyle, WS_EX_CLIENTEDGE, IdInputEdit, hwnd);
        g_outputEdit = createControl(L"EDIT", L"", editStyle | ES_READONLY, WS_EX_CLIENTEDGE, IdOutputEdit, hwnd);

        SetWindowSubclass(g_inputEdit, inputEditSubclassProc, 1U, 0);
        SetWindowSubclass(g_outputEdit, outputEditSubclassProc, 1U, 0);
        DragAcceptFiles(g_inputEdit, TRUE);

        SendMessageW(g_inputEdit, EM_SETLIMITTEXT, 0, 0);
        SendMessageW(g_outputEdit, EM_SETLIMITTEXT, 0, 0);

        g_progressBar = createControl(
            PROGRESS_CLASSW,
            L"",
            PBS_SMOOTH,
            0,
            IdProgressBar,
            hwnd);
        ShowWindow(g_progressBar, SW_HIDE);

        g_statusLabel = createControl(L"STATIC", L"Ready.", 0, 0, IdStatusLabel, hwnd);

        loadPersistentSettings();
        updateModeUi();
        layoutControls(hwnd);
    }

    LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
            case WM_CREATE:
                g_mainWindow = hwnd;
                createUi(hwnd);
                return 0;

            case WM_SIZE:
                layoutControls(hwnd);
                return 0;

            case WM_COMMAND:
            {
                const int id = LOWORD(wParam);
                const int notify = HIWORD(wParam);

                if (handleOptionsMenuCommand(id, false))
                    return 0;

                if (id == IdConvertButton && notify == BN_CLICKED)
                {
                    convertNow();
                    return 0;
                }
                if (id == IdCopyButton && notify == BN_CLICKED)
                {
                    copyOutputToClipboard();
                    return 0;
                }
                if (id == IdClearButton && notify == BN_CLICKED)
                {
                    clearText();
                    return 0;
                }
                if (id == IdSwapButton && notify == BN_CLICKED)
                {
                    swapOutputToInput();
                    return 0;
                }
                if ((id == IdInputTypeCombo || id == IdOutputTypeCombo || id == IdKeyCombo || id == IdPayloadKeyCombo) && notify == CBN_SELCHANGE)
                {
                    updateModeUi();
                    savePersistentSettings();
                    if (isChecked(g_autoCheck))
                        convertNow();
                    return 0;
                }
                if ((id == IdGroupCheck || id == IdAnnotateCheck || id == IdCmpOutputCheck ||
                     id == IdDuckStationCombineCheck || id == IdDuckStationCondenseCheck ||
                     id == IdSerialRepeaterCondenseCheck) && notify == BN_CLICKED)
                {
                    updateOptionsMenu();
                    savePersistentSettings();
                    if (isChecked(g_autoCheck))
                        convertNow();
                    return 0;
                }
                if (id == IdInputEdit && notify == EN_CHANGE && isChecked(g_autoCheck) && !g_isConverting && !g_isLoadingInput)
                {
                    convertNow();
                    return 0;
                }
                return 0;
            }

            case WM_MENUSELECT:
                if (HIWORD(wParam) == 0xFFFFU || reinterpret_cast<HMENU>(lParam) == nullptr)
                {
                    g_selectedMenu = nullptr;
                    g_selectedMenuItem = 0U;
                    g_selectedMenuFlags = 0U;
                }
                else
                {
                    g_selectedMenu = reinterpret_cast<HMENU>(lParam);
                    g_selectedMenuFlags = HIWORD(wParam);
                    if ((g_selectedMenuFlags & MF_POPUP) != 0U)
                    {
                        const UINT position = LOWORD(wParam);
                        g_selectedMenuItem = GetMenuItemID(g_selectedMenu, static_cast<int>(position));
                    }
                    else
                    {
                        g_selectedMenuItem = LOWORD(wParam);
                    }
                }
                return 0;

            case WM_ENTERMENULOOP:
                if (g_optionsMenuMessageHook == nullptr)
                {
                    g_optionsMenuMessageHook = SetWindowsHookExW(
                        WH_MSGFILTER,
                        optionsMenuMessageFilter,
                        nullptr,
                        GetCurrentThreadId());
                }
                return 0;

            case WM_EXITMENULOOP:
                if (g_optionsMenuMessageHook != nullptr)
                {
                    UnhookWindowsHookEx(g_optionsMenuMessageHook);
                    g_optionsMenuMessageHook = nullptr;
                }
                g_selectedMenu = nullptr;
                g_selectedMenuItem = 0U;
                g_selectedMenuFlags = 0U;

                if (g_pendingButtonVisibilityUpdate)
                {
                    g_pendingButtonVisibilityUpdate = false;
                    applyButtonVisibility();
                    layoutControls(hwnd);
                }
                return 0;

            case WM_CLOSE:
                if (g_isBatchDecrypting)
                {
                    setStatus(L"Please wait for the folder batch to finish before closing.");
                    MessageBeep(MB_ICONWARNING);
                    return 0;
                }
                savePersistentSettings();
                DestroyWindow(hwnd);
                return 0;

            case WM_DESTROY:
                if (g_optionsMenuMessageHook != nullptr)
                {
                    UnhookWindowsHookEx(g_optionsMenuMessageHook);
                    g_optionsMenuMessageHook = nullptr;
                }
                PostQuitMessage(0);
                return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (wc.hIcon == nullptr)
        wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    if (wc.hIconSm == nullptr)
        wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = WindowClassName;

    if (RegisterClassExW(&wc) == 0)
        return 1;

    WNDCLASSEXW splitterClass{};
    splitterClass.cbSize = sizeof(splitterClass);
    splitterClass.lpfnWndProc = splitterProc;
    splitterClass.hInstance = instance;
    splitterClass.hCursor = LoadCursorW(nullptr, IDC_SIZEWE);
    splitterClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1);
    splitterClass.lpszClassName = SplitterClassName;

    if (RegisterClassExW(&splitterClass) == 0)
        return 1;

    HWND hwnd = CreateWindowExW(
        0,
        WindowClassName,
        WindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1120,
        720,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (hwnd == nullptr)
        return 1;

    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
