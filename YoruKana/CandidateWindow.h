#pragma once

#include <windows.h>

#include <functional>
#include <limits>
#include <string>
#include <vector>

// ===== 1. 候補ウィンドウ =====
// 変換候補 / 予測候補を入力位置 (composition のテキスト範囲) の直下に表示する
// 自前描画のポップアップ。フォーカスを奪わない (WS_EX_NOACTIVATE) ため、
// 表示中もキー入力はアプリ側へ届き続ける。
// 変換中は上部に [変換] [予測] のタブを持ち、クリックでも切り替えられる。
// 1ページ9件まで表示し、選択中候補を含むページを自動で切り替える。
// クリックで候補を選ぶこともできる (OnItemClicked に絶対インデックスが渡る)。
//
// このクラスのメソッドはすべて TIP のスレッド (ホストアプリの UI スレッド)
// から呼ぶこと。TSF のコールバックと同じスレッドで動作する前提。
class CCandidateWindow {
public:
    // 選択なしを表す番兵 (入力中の予測表示など、強調表示が無い状態)。
    static constexpr size_t kNoSelection = std::numeric_limits<size_t>::max();

    CCandidateWindow();
    ~CCandidateWindow();

    // ウィンドウを作成する (Activate 時に呼ぶ)。既に作成済みなら何もしない。
    bool Create(HINSTANCE hInstance);
    // ウィンドウを破棄する (Deactivate 時に呼ぶ)。
    void DestroyWindowHandle();

    // 候補一覧を表示する。anchor は入力位置の画面座標矩形。
    // showTabs=true のとき [変換][予測] タブを描き、activeTab (0=変換, 1=予測) を強調する。
    // showTabs=false のときは headerLabel を単独の見出しとして描く (nullptr なら見出しなし)。
    void Show(const std::vector<std::wstring>& items, size_t selected, const RECT& anchor,
              bool showTabs, int activeTab, const wchar_t* headerLabel);
    // 選択位置だけを更新する (ページが変わる場合は位置も再計算する)。
    void SetSelection(size_t selected);
    void Hide();
    bool IsVisible() const { return _hwnd != nullptr && ::IsWindowVisible(_hwnd); }
    // 現在表示中のページの先頭候補の絶対インデックス
    // (数字キーによるページ内番号指定を絶対位置へ直すために使う)。
    size_t PageStart() const {
        return (_selected == kNoSelection) ? 0 : (_selected / kPageSize) * kPageSize;
    }

    // クリックで候補が選ばれたときのコールバック (候補の絶対インデックス)。
    std::function<void(size_t)> OnItemClicked;
    // タブがクリックされたときのコールバック (0=変換, 1=予測)。
    std::function<void(int)> OnTabClicked;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint();
    void Layout(); // _anchor と選択ページから位置と大きさを決めて配置する

    static constexpr size_t kPageSize = 9;

    size_t PageCount() const {
        return (_items.size() + kPageSize - 1) / kPageSize;
    }
    int HeaderHeight() const { return (_showTabs || _headerLabel != nullptr) ? _rowHeight : 0; }

    HWND _hwnd = nullptr;
    HFONT _font = nullptr;
    int _rowHeight = 24;
    int _numberWidth = 24;
    std::vector<std::wstring> _items;
    size_t _selected = kNoSelection;
    RECT _anchor = {};
    bool _showTabs = false;
    int _activeTab = 0;
    const wchar_t* _headerLabel = nullptr;
    RECT _tabRect[2] = {}; // 描画時に記録するタブのクリック領域
};
