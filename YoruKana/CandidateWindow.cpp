#include "CandidateWindow.h"

#include <algorithm>

namespace {

const wchar_t kWindowClassName[] = L"YoruKanaCandidateWindow";
bool g_classRegistered = false;

} // namespace

// ===== 1. 生成・破棄 =====
CCandidateWindow::CCandidateWindow() = default;

CCandidateWindow::~CCandidateWindow() {
    DestroyWindowHandle();
}

bool CCandidateWindow::Create(HINSTANCE hInstance) {
    if (_hwnd != nullptr) return true;

    if (!g_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &CCandidateWindow::WndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kWindowClassName;
        if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
        g_classRegistered = true;
    }

    _hwnd = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                            kWindowClassName, nullptr, WS_POPUP, 0, 0, 0, 0, nullptr,
                            nullptr, hInstance, this);
    if (_hwnd == nullptr) return false;

    // メッセージフォント (メニュー等と同じ書体) を使う。
    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(ncm);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
        _font = CreateFontIndirectW(&ncm.lfMessageFont);
    }
    if (_font != nullptr) {
        HDC hdc = GetDC(_hwnd);
        if (hdc != nullptr) {
            HGDIOBJ old = SelectObject(hdc, _font);
            TEXTMETRICW tm = {};
            GetTextMetricsW(hdc, &tm);
            _rowHeight = tm.tmHeight + 8;
            SIZE sz = {};
            GetTextExtentPoint32W(hdc, L"9.", 2, &sz);
            _numberWidth = sz.cx + 12;
            SelectObject(hdc, old);
            ReleaseDC(_hwnd, hdc);
        }
    }
    return true;
}

void CCandidateWindow::DestroyWindowHandle() {
    if (_hwnd != nullptr) {
        ::DestroyWindow(_hwnd);
        _hwnd = nullptr;
    }
    if (_font != nullptr) {
        DeleteObject(_font);
        _font = nullptr;
    }
}

// ===== 2. 表示制御 =====
void CCandidateWindow::Show(const std::vector<std::wstring>& items, size_t selected,
                            const RECT& anchor, bool showTabs, int activeTab,
                            const wchar_t* headerLabel) {
    if (_hwnd == nullptr || items.empty()) return;
    _items = items;
    _selected = (selected == kNoSelection || selected < items.size()) ? selected
                                                                       : items.size() - 1;
    _anchor = anchor;
    _showTabs = showTabs;
    _activeTab = activeTab;
    _headerLabel = headerLabel;
    Layout();
    ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
    InvalidateRect(_hwnd, nullptr, FALSE);
}

void CCandidateWindow::SetSelection(size_t selected) {
    if (_hwnd == nullptr || _items.empty()) return;
    size_t oldPage = PageStart();
    _selected = (selected == kNoSelection || selected < _items.size()) ? selected
                                                                        : _items.size() - 1;
    if (PageStart() != oldPage) {
        Layout();
    }
    InvalidateRect(_hwnd, nullptr, FALSE);
}

void CCandidateWindow::Hide() {
    if (_hwnd != nullptr) ShowWindow(_hwnd, SW_HIDE);
}

void CCandidateWindow::Layout() {
    size_t start = PageStart();
    size_t rows = (std::min)(kPageSize, _items.size() - start);
    int height = HeaderHeight() + static_cast<int>(rows) * _rowHeight + 2;

    // 幅は表示中の候補の最長テキストに合わせる。
    int width = 160;
    HDC hdc = GetDC(_hwnd);
    if (hdc != nullptr) {
        HGDIOBJ old = (_font != nullptr) ? SelectObject(hdc, _font) : nullptr;
        for (size_t i = start; i < start + rows; i++) {
            SIZE sz = {};
            GetTextExtentPoint32W(hdc, _items[i].c_str(),
                                  static_cast<int>(_items[i].size()), &sz);
            width = (std::max)(width, static_cast<int>(_numberWidth + sz.cx + 16));
        }
        if (old != nullptr) SelectObject(hdc, old);
        ReleaseDC(_hwnd, hdc);
    }
    width = (std::min)(width, 560);

    int x = _anchor.left;
    int y = _anchor.bottom + 2;
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromRect(&_anchor, MONITOR_DEFAULTTONEAREST), &mi)) {
        if (y + height > mi.rcWork.bottom) y = _anchor.top - height - 2; // 下に入らなければ上へ
        if (y < mi.rcWork.top) y = mi.rcWork.top;
        if (x + width > mi.rcWork.right) x = mi.rcWork.right - width;
        if (x < mi.rcWork.left) x = mi.rcWork.left;
    }
    SetWindowPos(_hwnd, HWND_TOPMOST, x, y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

// ===== 3. 描画 =====
void CCandidateWindow::OnPaint() {
    PAINTSTRUCT ps = {};
    HDC hdc = BeginPaint(_hwnd, &ps);
    RECT rc = {};
    GetClientRect(_hwnd, &rc);

    HBRUSH bg = GetSysColorBrush(COLOR_WINDOW);
    FillRect(hdc, &rc, bg);
    HBRUSH border = CreateSolidBrush(RGB(0x8a, 0x8a, 0x8a));
    FrameRect(hdc, &rc, border);
    DeleteObject(border);

    HGDIOBJ oldFont = (_font != nullptr) ? SelectObject(hdc, _font) : nullptr;
    SetBkMode(hdc, TRANSPARENT);
    int top = 1;

    // ----- 3.1 ヘッダー (タブ / 見出し + ページ表示) -----
    if (HeaderHeight() > 0) {
        RECT header = {1, 1, rc.right - 1, 1 + _rowHeight};
        FillRect(hdc, &header, GetSysColorBrush(COLOR_BTNFACE));
        if (_showTabs) {
            const wchar_t* labels[2] = {L"変換", L"予測"};
            int x = header.left + 4;
            for (int t = 0; t < 2; t++) {
                SIZE sz = {};
                GetTextExtentPoint32W(hdc, labels[t], 2, &sz);
                RECT tab = {x, header.top + 2, x + sz.cx + 16, header.bottom - 2};
                _tabRect[t] = tab;
                if (t == _activeTab) {
                    FillRect(hdc, &tab, GetSysColorBrush(COLOR_HIGHLIGHT));
                    SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
                } else {
                    SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
                }
                DrawTextW(hdc, labels[t], 2, &tab,
                          DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
                x = tab.right + 4;
            }
        } else if (_headerLabel != nullptr) {
            SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
            RECT labelRc = {header.left + 8, header.top, header.right - 8, header.bottom};
            DrawTextW(hdc, _headerLabel, -1, &labelRc,
                      DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        }
        // 複数ページあるときは右端にページ番号を出す。
        if (PageCount() > 1) {
            size_t page = (_selected == kNoSelection) ? 1 : (_selected / kPageSize) + 1;
            std::wstring pageText =
                std::to_wstring(page) + L"/" + std::to_wstring(PageCount());
            SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
            RECT pageRc = {header.left, header.top, header.right - 8, header.bottom};
            DrawTextW(hdc, pageText.c_str(), static_cast<int>(pageText.size()), &pageRc,
                      DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
        }
        top += _rowHeight;
    }

    // ----- 3.2 候補行 -----
    size_t start = PageStart();
    size_t rows = (std::min)(kPageSize, _items.size() - start);
    for (size_t i = 0; i < rows; i++) {
        size_t index = start + i;
        RECT row = {1, top + static_cast<LONG>(i) * _rowHeight, rc.right - 1,
                    top + static_cast<LONG>(i + 1) * _rowHeight};
        bool selected = (index == _selected);
        if (selected) {
            FillRect(hdc, &row, GetSysColorBrush(COLOR_HIGHLIGHT));
        }
        SetTextColor(hdc, GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
        std::wstring num = std::to_wstring(i + 1) + L".";
        RECT numRc = {row.left + 6, row.top, row.left + _numberWidth, row.bottom};
        DrawTextW(hdc, num.c_str(), static_cast<int>(num.size()), &numRc,
                  DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        RECT textRc = {row.left + _numberWidth, row.top, row.right - 6, row.bottom};
        DrawTextW(hdc, _items[index].c_str(), static_cast<int>(_items[index].size()),
                  &textRc, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    if (oldFont != nullptr) SelectObject(hdc, oldFont);
    EndPaint(_hwnd, &ps);
}

// ===== 4. ウィンドウプロシージャ =====
LRESULT CALLBACK CCandidateWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                           LPARAM lParam) {
    CCandidateWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<CCandidateWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        // 生成直後のメッセージ (WM_NCCREATE 以降) でも使えるよう、ここで保持する
        // (CreateWindowExW の戻り値が返る前からメッセージが届くため)。
        // 破棄中のメッセージで復活しないよう、設定はこの分岐でのみ行う。
        if (self != nullptr) self->_hwnd = hwnd;
    } else {
        self = reinterpret_cast<CCandidateWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self != nullptr) {
        return self->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CCandidateWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT:
            OnPaint();
            return 0;
        case WM_ERASEBKGND:
            return 1; // OnPaint で全面描画するため背景消去は不要
        case WM_LBUTTONUP: {
            int x = static_cast<short>(LOWORD(lParam));
            int y = static_cast<short>(HIWORD(lParam));
            int headerHeight = HeaderHeight();
            if (_showTabs && y >= 1 && y < 1 + headerHeight) {
                POINT pt = {x, y};
                for (int t = 0; t < 2; t++) {
                    if (PtInRect(&_tabRect[t], pt)) {
                        if (t != _activeTab && OnTabClicked) OnTabClicked(t);
                        return 0;
                    }
                }
                return 0;
            }
            if (y < 1 + headerHeight) return 0;
            size_t row = static_cast<size_t>(y - 1 - headerHeight) /
                         static_cast<size_t>(_rowHeight);
            // 表示中ページの行数を超える位置 (下端の境界付近) は候補として扱わない
            // (次ページの見えていない候補を確定してしまうのを防ぐ)。
            size_t start = PageStart();
            size_t rows = (std::min)(kPageSize, _items.size() - start);
            if (row < rows && OnItemClicked) OnItemClicked(start + row);
            return 0;
        }
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE; // クリックされてもフォーカスを奪わない
        default:
            return DefWindowProcW(_hwnd, msg, wParam, lParam);
    }
}
