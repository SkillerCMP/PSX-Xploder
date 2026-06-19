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
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "XploderCmpConverter.hpp"

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
    constexpr wchar_t AppVersion[] = L"v1.01";
    constexpr wchar_t WindowTitle[] = L"Xploder PSX Converter v1.01";

    enum ControlId : int
    {
        IdInputEdit = 1001,
        IdOutputEdit,
        IdModeCombo,
        IdKeyCombo,
        IdPayloadKeyCombo,
        IdGroupCheck,
        IdAnnotateCheck,
        IdPrefixCheck,
        IdAutoCheck,
        IdConvertButton,
        IdCopyButton,
        IdClearButton,
        IdSwapButton,
        IdSplitter,
        IdStatusLabel
    };

    HWND g_mainWindow = nullptr;
    HWND g_inputEdit = nullptr;
    HWND g_outputEdit = nullptr;
    HWND g_modeCombo = nullptr;
    HWND g_keyCombo = nullptr;
    HWND g_payloadKeyCombo = nullptr;
    HWND g_groupCheck = nullptr;
    HWND g_annotateCheck = nullptr;
    HWND g_prefixCheck = nullptr;
    HWND g_autoCheck = nullptr;
    HWND g_convertButton = nullptr;
    HWND g_copyButton = nullptr;
    HWND g_clearButton = nullptr;
    HWND g_swapButton = nullptr;
    HWND g_statusLabel = nullptr;
    HWND g_splitter = nullptr;
    HWND g_inputLabel = nullptr;
    HWND g_outputLabel = nullptr;
    HFONT g_uiFont = nullptr;
    bool g_isConverting = false;
    bool g_isLoadingInput = false;
    bool g_isDraggingSplitter = false;
    double g_splitRatio = 0.5;

    constexpr int LayoutMargin = 10;
    constexpr int SplitterWidth = 8;
    constexpr int MinimumPaneWidth = 100;

    void layoutControls(HWND hwnd);
    void layoutPaneControls(HWND hwnd, bool repaintImmediately);
    void convertNow();

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

            if (ch == L'\n')
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
        if (msg == WM_DROPFILES)
        {
            HDROP drop = reinterpret_cast<HDROP>(wParam);
            const UINT fileCount = DragQueryFileW(drop, 0xFFFFFFFFU, nullptr, 0);

            if (fileCount != 1U)
            {
                setStatus(L"Drop one text file at a time onto the Input pane.");
                MessageBeep(MB_ICONWARNING);
                DragFinish(drop);
                return 0;
            }

            const UINT pathLength = DragQueryFileW(drop, 0U, nullptr, 0);
            std::wstring path(static_cast<std::size_t>(pathLength) + 1U, L'\0');
            DragQueryFileW(drop, 0U, path.data(), pathLength + 1U);
            path.resize(pathLength);

            DragFinish(drop);
            loadDroppedTextFile(path);
            return 0;
        }

        if (msg == WM_NCDESTROY)
        {
            DragAcceptFiles(hwnd, FALSE);
            RemoveWindowSubclass(hwnd, inputEditSubclassProc, subclassId);
        }

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

    xploder_converter::Options selectedOptions()
    {
        xploder_converter::Options options;
        options.mode = comboIndex(g_modeCombo) == 1
            ? xploder_converter::Mode::Encrypt
            : xploder_converter::Mode::Decrypt;
        options.encryptionKey = selectedEncryptionKey();
        options.massWritePayloadKey = comboIndex(g_payloadKeyCombo) == 1 ? 7 : 6;
        options.groupEncryptedOutput = isChecked(g_groupCheck);
        options.annotateCodeTypes = isChecked(g_annotateCheck);
        options.prefixPlainNames = isChecked(g_prefixCheck);
        return options;
    }

    void updateModeUi()
    {
        const bool encrypt = comboIndex(g_modeCombo) == 1;
        EnableWindow(g_keyCombo, encrypt ? TRUE : FALSE);
        EnableWindow(g_groupCheck, encrypt ? TRUE : FALSE);
        // The selected Type 5 payload key is used when encrypting both normal
        // and nested active Type 5 blocks. Decrypt reads the key from encrypted headers.
        EnableWindow(g_payloadKeyCombo, encrypt ? TRUE : FALSE);
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
            const xploder_converter::Options options = selectedOptions();
            const std::string outputUtf8 = xploder_converter::convertText(inputUtf8, options);
            setEditText(g_outputEdit, widenUtf8(outputUtf8));

            const wchar_t* mode = options.mode == xploder_converter::Mode::Encrypt ? L"Encrypt" : L"Decrypt";
            setStatus(std::wstring(L"Done: ") + mode + L" conversion complete.");
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
        setStatus(L"Output moved to input.");
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
        const int controlsH = 76;
        const int labelH = 20;
        const int statusH = 24;
        const int editorTop = top + controlsH + labelH + 4;
        const int editorBottom = height - statusH - margin;
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

        int x = margin;
        int y = top;
        const int comboW = 210;

        MoveWindow(g_modeCombo, x, y + 18, comboW, 200, TRUE);

        x += comboW + gap;
        MoveWindow(g_keyCombo, x, y + 18, 150, 200, TRUE);
        x += 150 + gap;
        MoveWindow(g_payloadKeyCombo, x, y + 18, 220, 200, TRUE);
        x += 220 + gap;
        MoveWindow(g_convertButton, x, y + 16, 90, 28, TRUE);
        x += 94;
        MoveWindow(g_copyButton, x, y + 16, 105, 28, TRUE);
        x += 109;
        MoveWindow(g_swapButton, x, y + 16, 115, 28, TRUE);
        x += 119;
        MoveWindow(g_clearButton, x, y + 16, 75, 28, TRUE);

        // Static option labels are omitted to keep the layout simple; combo text is descriptive.
        x = margin;
        y = top + 50;
        MoveWindow(g_groupCheck, x, y, 190, 22, TRUE);
        x += 200;
        MoveWindow(g_annotateCheck, x, y, 150, 22, TRUE);
        x += 160;
        MoveWindow(g_prefixCheck, x, y, 175, 22, TRUE);
        x += 185;
        MoveWindow(g_autoCheck, x, y, 115, 22, TRUE);

        layoutPaneControls(hwnd, true);
        MoveWindow(g_statusLabel, margin, height - statusH, width - margin * 2, statusH, TRUE);
    }

    void createUi(HWND hwnd)
    {
        INITCOMMONCONTROLSEX icc{};
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icc);

        g_uiFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        g_modeCombo = createControl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, IdModeCombo, hwnd);
        addComboItem(g_modeCombo, L"Decrypt: XplorerPro/FX -> Active RAW");
        addComboItem(g_modeCombo, L"Encrypt: Active RAW -> XplorerPro/FX");
        SendMessageW(g_modeCombo, CB_SETCURSEL, 0, 0);

        g_keyCombo = createControl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, IdKeyCombo, hwnd);
        addComboItem(g_keyCombo, L"Encrypt Key 4 / WHBX style");
        addComboItem(g_keyCombo, L"Encrypt Key 5 / WB123 style");
        addComboItem(g_keyCombo, L"Encrypt Key 6 / AB+XOR style");
        addComboItem(g_keyCombo, L"Encrypt Key 7 / FCD! style");
        SendMessageW(g_keyCombo, CB_SETCURSEL, 1, 0);

        g_payloadKeyCombo = createControl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, IdPayloadKeyCombo, hwnd);
        addComboItem(g_payloadKeyCombo, L"Type 5 key 6 (normal + nested)");
        addComboItem(g_payloadKeyCombo, L"Type 5 key 7 (normal + nested)");
        SendMessageW(g_payloadKeyCombo, CB_SETCURSEL, 0, 0);

        g_convertButton = createControl(L"BUTTON", L"Convert", BS_PUSHBUTTON | WS_TABSTOP, 0, IdConvertButton, hwnd);
        g_copyButton = createControl(L"BUTTON", L"Copy Output", BS_PUSHBUTTON | WS_TABSTOP, 0, IdCopyButton, hwnd);
        g_swapButton = createControl(L"BUTTON", L"Output -> Input", BS_PUSHBUTTON | WS_TABSTOP, 0, IdSwapButton, hwnd);
        g_clearButton = createControl(L"BUTTON", L"Clear", BS_PUSHBUTTON | WS_TABSTOP, 0, IdClearButton, hwnd);

        g_groupCheck = createControl(L"BUTTON", L"Group encrypted 4-4-4", BS_AUTOCHECKBOX | WS_TABSTOP, 0, IdGroupCheck, hwnd);
        g_annotateCheck = createControl(L"BUTTON", L"Annotate code types", BS_AUTOCHECKBOX | WS_TABSTOP, 0, IdAnnotateCheck, hwnd);
        g_prefixCheck = createControl(L"BUTTON", L"Add + to plain names", BS_AUTOCHECKBOX | WS_TABSTOP, 0, IdPrefixCheck, hwnd);
        SendMessageW(g_prefixCheck, BM_SETCHECK, BST_CHECKED, 0);
        g_autoCheck = createControl(L"BUTTON", L"Auto Convert", BS_AUTOCHECKBOX | WS_TABSTOP, 0, IdAutoCheck, hwnd);

        g_inputLabel = createControl(L"STATIC", L"Input (drop a text file here)", 0, 0, 0, hwnd);
        g_outputLabel = createControl(L"STATIC", L"Output", 0, 0, 0, hwnd);
        g_splitter = createControl(SplitterClassName, L"", 0, 0, IdSplitter, hwnd);

        const DWORD editStyle = WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN;
        g_inputEdit = createControl(L"EDIT", L"", editStyle, WS_EX_CLIENTEDGE, IdInputEdit, hwnd);
        g_outputEdit = createControl(L"EDIT", L"", editStyle | ES_READONLY, WS_EX_CLIENTEDGE, IdOutputEdit, hwnd);

        SetWindowSubclass(g_inputEdit, inputEditSubclassProc, 1U, 0);
        DragAcceptFiles(g_inputEdit, TRUE);

        SendMessageW(g_inputEdit, EM_SETLIMITTEXT, 0, 0);
        SendMessageW(g_outputEdit, EM_SETLIMITTEXT, 0, 0);

        g_statusLabel = createControl(L"STATIC", L"Ready.", 0, 0, IdStatusLabel, hwnd);

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
                if ((id == IdModeCombo || id == IdKeyCombo || id == IdPayloadKeyCombo) && notify == CBN_SELCHANGE)
                {
                    updateModeUi();
                    if (isChecked(g_autoCheck))
                        convertNow();
                    return 0;
                }
                if ((id == IdGroupCheck || id == IdAnnotateCheck || id == IdPrefixCheck) && notify == BN_CLICKED)
                {
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

            case WM_DESTROY:
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
