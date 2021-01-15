/************************************************************/
/*                                                          */
/*        RunCPM + VT100 Emulator for Wio Termainal         */
/*                                                          */
/*   Wio_RunCPM_vt100_CardKB                                */
/*     https://github.com/robo8080/Wio_RunCPM_vt100_CardKB  */
/*                                                          */
/*   RunCPM - Z80 CP/M 2.2 emulator                         */
/*     https://github.com/MockbaTheBorg/RunCPM              */
/*   VT100 Terminal Emulator for Wio Terminal               */
/*     https://github.com/ht-deko/vt100_wt                  */
/*                                                          */
/************************************************************/

//------VT100_WT---------------------------------------------------------

#include <Arduino.h>
#include <Seeed_Arduino_FreeRTOS.h>
// https://github.com/Seeed-Studio/Seeed_Arduino_FreeRTOS
#include <SPI.h>
#include <LovyanGFX.hpp>
// https://github.com/lovyan03/LovyanGFX
#include <SAMD51_InterruptTimer.h>
// https://github.com/Dennis-van-Gils/SAMD51_InterruptTimer
#include <Reset.h>
#include <Wire.h>

//------RunCPM-----------------------------------------------------------

#include "globals.h"
#include <SdFat.h>
// https://github.com/greiman/SdFat
#include "hardware/wioterm.h"
#include "abstraction_arduino.h"

// Serial port speed
#define SERIALSPD 9600

// PUN: device configuration
#ifdef USE_PUN
File pun_dev;
int pun_open = FALSE;
#endif

// LST: device configuration
#ifdef USE_LST
File lst_dev;
int lst_open = FALSE;
#endif

#include "ram.h"
#include "console.h"
#include "cpu.h"
#include "disk.h"
#include "host.h"
#include "cpm.h"
#ifdef CCP_INTERNAL
#include "ccp.h"
#endif

//------Settings---------------------------------------------------------

// フォント
//#include <font6x8tt.h>            // 6x8 ドットフォント (TTBASIC 付属)
//#include "font6x8e200.h"          // 6x8 ドットフォント (SHARP PC-E200 風)
//#include "font6x8e500.h"          // 6x8 ドットフォント (SHARP PC-E500 風)
#include "font6x8sc1602b.h"       // 6x8 ドットフォント (SUNLIKE SC1602B 風)

// フォント管理用
#define CH_W          6           // フォント横サイズ
#define CH_H          8           // フォント縦サイズ

// スクリーン管理用
#define RSP_W         320         // 実ピクセルスクリーン横サイズ
#define RSP_H         240         // 実ピクセルスクリーン縦サイズ
#define SC_W          52          // キャラクタスクリーン横サイズ (<= 53)
#define SC_H          29          // キャラクタスクリーン縦サイズ (<= 30)

// 色
#define FORE_COLOR    clWhite     // 初期前景色
#define BACK_COLOR    clBlue      // 初期背景色
#define CURSOR_COLOR  clWhite     // カーソル色

// エスケープシーケンス
#define USE_EGR                   // EGR 拡張

// スピーカー制御用ピン
#define SPK_PIN       WIO_BUZZER  // Wio Terminal

//-----------------------------------------------------------------------

// 交換
#define swap(a, b) { uint16_t t = a; a = b; b = t; }

// シリアル
#define DebugSerial Serial1

// LED 制御用ピン
/*
  #define LED_01  D2
  #define LED_02  D3
  #define LED_03  D4
  #define LED_04  D5
*/

// 文字アトリビュート用
struct TATTR {
  uint8_t Bold  : 1;      // 1
  uint8_t Faint  : 1;     // 2
  uint8_t Italic : 1;     // 3
  uint8_t Underline : 1;  // 4
  uint8_t Blink : 1;      // 5 (Slow Blink)
  uint8_t RapidBlink : 1; // 6
  uint8_t Reverse : 1;    // 7
  uint8_t Conceal : 1;    // 8
};

union ATTR {
  uint8_t value;
  struct TATTR Bits;
};

// カラーアトリビュート用 (RGB565)
PROGMEM const uint16_t aColors[] = {
  // Normal (0..7)
  0x0000, // black
  0x8000, // red
  0x0400, // green
  0x8400, // yellow
  0x0010, // blue (Dark)
  0x8010, // magenta
  0x0410, // cyan
  0xbdf7, // black
  // Bright (8..15)
  0x8410, // white
  0xf800, // red
  0x07e0, // green
  0xffe0, // yellow
  0x001f, // blue
  0xf81f, // magenta
  0x07ff, // cyan
  0xe73c  // white
};

struct TCOLOR {
  uint8_t Foreground : 4;
  uint8_t Background : 4;
};
union COLOR {
  uint8_t value;
  struct TCOLOR Color;
};

// 環境設定用
struct TMODE {
  bool Reserved2 : 1;  // 2
  bool Reserved4 : 1;  // 4:
  bool Reserved12 : 1; // 12:
  bool CrLf : 1;       // 20: LNM (Line feed new line mode)
  bool Reserved33 : 1; // 33:
  bool Reserved34 : 1; // 34:
  uint8_t Reverse : 2;
};

union MODE {
  uint8_t value;
  struct TMODE Flgs;
};

struct TMODE_EX {
  bool Reserved1 : 1;     // 1 DECCKM (Cursor Keys Mode)
  bool Reserved2 : 1;     // 2 DECANM (ANSI/VT52 Mode)
  bool Reserved3 : 1;     // 3 DECCOLM (Column Mode)
  bool Reserved4 : 1;     // 4 DECSCLM (Scrolling Mode)
  bool ScreenReverse : 1; // 5 DECSCNM (Screen Mode)
  bool Reserved6 : 1;     // 6 DECOM (Origin Mode)
  bool WrapLine  : 1;     // 7 DECAWM (Autowrap Mode)
  bool Reserved8 : 1;     // 8 DECARM (Auto Repeat Mode)
  bool Reserved9 : 1;     // 9 DECINLM (Interlace Mode)
  uint16_t Reverse : 7;
};

union MODE_EX {
  uint16_t value;
  struct TMODE_EX Flgs;
};

// 座標やサイズのプレ計算
PROGMEM const uint16_t SCSIZE      = SC_W * SC_H;        // キャラクタスクリーンサイズ
PROGMEM const uint16_t SP_W        = SC_W * CH_W;        // ピクセルスクリーン横サイズ
PROGMEM const uint16_t SP_H        = SC_H * CH_H;        // ピクセルスクリーン縦サイズ
PROGMEM const uint16_t MAX_CH_X    = CH_W - 1;           // フォント最大横位置
PROGMEM const uint16_t MAX_CH_Y    = CH_H - 1;           // フォント最大縦位置
PROGMEM const uint16_t MAX_SC_X    = SC_W - 1;           // キャラクタスクリーン最大横位置
PROGMEM const uint16_t MAX_SC_Y    = SC_H - 1;           // キャラクタスクリーン最大縦位置
PROGMEM const uint16_t MAX_SP_X    = SP_W - 1;           // ピクセルスクリーン最大横位置
PROGMEM const uint16_t MAX_SP_Y    = SP_H - 1;           // ピクセルスクリーン最大縦位置
PROGMEM const uint16_t MARGIN_LEFT = (RSP_W - SP_W) / 2; // 左マージン
PROGMEM const uint16_t MARGIN_TOP  = (RSP_H - SP_H) / 2; // 上マージン

// 色
PROGMEM const uint8_t clBlack   = 0;
PROGMEM const uint8_t clRed     = 1;
PROGMEM const uint8_t clGreen   = 2;
PROGMEM const uint8_t clYellow  = 3;
PROGMEM const uint8_t clBlue    = 4;
PROGMEM const uint8_t clMagenta = 5;
PROGMEM const uint8_t clCyan    = 6;
PROGMEM const uint8_t clWhite   = 7;

// デフォルト
PROGMEM const uint8_t defaultMode = 0b00001000;
PROGMEM const uint16_t defaultModeEx = 0b0000000001000000;
PROGMEM const union ATTR defaultAttr = {0b00000000};
PROGMEM const union COLOR defaultColor = {(BACK_COLOR << 4) | FORE_COLOR};

// スクロール有効行
uint16_t M_TOP    = 0;        // スクロール行上限
uint16_t M_BOTTOM = MAX_SC_Y; // スクロール行下限

// フォント先頭アドレス
uint8_t* fontTop;

// バッファ
uint8_t screen[SCSIZE];       // スクリーンバッファ
uint8_t attrib[SCSIZE];       // 文字アトリビュートバッファ
uint8_t colors[SCSIZE];       // カラーアトリビュートバッファ
uint8_t tabs[SC_W];           // タブ位置バッファ

// 状態
PROGMEM enum class em {NONE,  ES, CSI, CSI2, LSC, G0S, G1S, EGR};
em escMode = em::NONE;         // エスケープシーケンスモード
bool isShowCursor = false;     // カーソル表示中か？
bool canShowCursor = false;    // カーソル表示可能か？
bool lastShowCursor = false;   // 前回のカーソル表示状態
bool hasParam = false;         // <ESC> [ がパラメータを持っているか？
bool isNegative = false;       // パラメータにマイナス符号が付いているか？
bool isDECPrivateMode = false; // DEC Private Mode (<ESC> [ ?)
union MODE mode = {defaultMode};
union MODE_EX mode_ex = {defaultModeEx};

/********************************************
  キーボードと Wio Terminal のボタンとスイッチの対応
  +-------------+--------------+-----------+
  | キーボード  | Wio Terminal |  ESC SEQ  |
  +-------------+--------------+-----------+
  | [F1]        | WIO_KEY_C    | [ESC] O P |
  | [F2]        | WIO_KEY_B    | [ESC] O Q |
  | [F3]        | WIO_KEY_A    | [ESC] O R |
  | [UP]        | WIO_5S_UP    | [ESC] O A |
  | [DOWN]      | WIO_5S_DOWN  | [ESC] O B |
  | [RIGHT]     | WIO_5S_RIGHT | [ESC] O C |
  | [LEFT]      | WIO_5S_LEFT  | [ESC] O D |
  | [ENTER]     | WIO_5S_PRESS | [CR]      |
  +-------------+--------------+-----------+
********************************************/

// コマンドの長さ
PROGMEM const int CMD_LEN = 4;

// キー
int key;
void printKey();
void printSpecialKey(const char *str);

// スイッチ情報
enum WIO_SW {SW_UP, SW_DOWN, SW_RIGHT, SW_LEFT, SW_PRESS};
PROGMEM const int SW_PORT[5] = {WIO_5S_UP, WIO_5S_DOWN, WIO_5S_RIGHT, WIO_5S_LEFT, WIO_5S_PRESS};
PROGMEM const char SW_CMD[5][CMD_LEN] = {"\eOA", "\eOB", "\eOC", "\eOD", "\r"};
bool prev_sw[5] = {false, false, false, false, false};

// ボタン情報
enum WIO_BTN {BT_A, BT_B, BT_C};
PROGMEM const int BTN_PORT[3] = {WIO_KEY_A, WIO_KEY_B, WIO_KEY_C};
PROGMEM const char BTN_CMD[3][CMD_LEN] = {"\eOR", "\eOQ", "\eOP"};
bool prev_btn[3] = {false, false, false};

// 特殊キー情報
enum SP_KEY {KY_HOME, KY_INS, KY_DEL, KY_END, KY_PGUP, KY_PGDOWN};
PROGMEM const char KEY_CMD[6][CMD_LEN] = {"\eO1", "\eO2", "\x7F", "\eO4", "\eO5", "\eO6"};

// 前回位置情報
int16_t p_XP = 0;
int16_t p_YP = 0;

// カレント情報
int16_t XP = 0;
int16_t YP = 0;
union ATTR cAttr   = defaultAttr;
union COLOR cColor = defaultColor;

// バックアップ情報
int16_t prev_XP = 0;
int16_t prev_YP = 0;
union ATTR bAttr   = defaultAttr;
union COLOR bColor = defaultColor;

// CSI / EGR パラメータ
int16_t nVals = 0;
int16_t vals[10] = {};

// カーソル描画用
bool needCursorUpdate = false;
bool hideCursor = false;

// LCD 制御用
static LGFX lcd;

// 関数
// -----------------------------------------------------------------------------

// 指定位置の文字の更新表示
void sc_updateChar(uint16_t x, uint16_t y) {
  uint16_t idx = SC_W * y + x;
  uint16_t buflen = CH_W * CH_H;
  uint16_t buf[buflen];
  uint8_t c    = screen[idx];        // キャラクタの取得
  uint8_t* ptr = &fontTop[c * CH_H]; // フォントデータ先頭アドレス
  union ATTR a;
  union COLOR l;
  a.value = attrib[idx];             // 文字アトリビュートの取得
  l.value = colors[idx];             // カラーアトリビュートの取得
  uint16_t fore = aColors[l.Color.Foreground | (a.Bits.Blink << 3)];
  uint16_t back = aColors[l.Color.Background | (a.Bits.Blink << 3)];

  if (a.Bits.Reverse) swap(fore, back);
  if (mode_ex.Flgs.ScreenReverse) swap(fore, back);
  lcd.setAddrWindow(MARGIN_LEFT + x * CH_W , MARGIN_TOP + y * CH_H, CH_W, CH_H);
  uint16_t cnt = 0;
  for (int i = 0; i < CH_H; i++) {
    bool prev = (a.Bits.Underline && (i == MAX_CH_Y));
    for (int j = 0; j < CH_W; j++) {
      bool pset = ((*ptr) & (0x80 >> j));
      buf[cnt] = (pset || prev) ? fore : back;
      if (a.Bits.Bold)
        prev = pset;
      cnt++;
    }
    ptr++;
  }
  lcd.pushPixels(buf, buflen, true);
}

// カーソルの描画
void drawCursor(uint16_t x, uint16_t y) {
  uint16_t buflen = CH_W * CH_H;
  uint16_t buf[buflen];

  lcd.setAddrWindow(MARGIN_LEFT + x * CH_W, MARGIN_TOP + y * CH_H, CH_W, CH_H);
  for (uint16_t i = 0; i < buflen; i++)
    buf[i] = aColors[CURSOR_COLOR];
  lcd.pushPixels(buf, buflen, true);
}

// カーソルの表示
void dispCursor(bool forceupdate) {
  if (escMode != em::NONE)
    return;
  if (hideCursor)
    return;
  if (!forceupdate)
    isShowCursor = !isShowCursor;
  if (isShowCursor)
    drawCursor(XP, YP);
  if (lastShowCursor || (forceupdate && isShowCursor))
    sc_updateChar(p_XP, p_YP);
  if (isShowCursor) {
    p_XP = XP;
    p_YP = YP;
  }
  if (!forceupdate)
    canShowCursor = false;
  lastShowCursor = isShowCursor;
  needCursorUpdate = false;
}

// 指定行をLCD画面に反映
// 引数
//  ln:行番号（0～29)
void sc_updateLine(uint16_t ln) {
  uint8_t c;
  uint8_t dt;
  uint16_t buf[2][SP_W];
  uint16_t cnt, idx;
  union ATTR a;
  union COLOR l;

  for (uint16_t i = 0; i < CH_H; i++) {            // 1文字高さ分ループ
    cnt = 0;
    for (uint16_t clm = 0; clm < SC_W; clm++) {    // 横文字数分ループ
      idx = ln * SC_W + clm;
      c  = screen[idx];                            // キャラクタの取得
      a.value = attrib[idx];                       // 文字アトリビュートの取得
      l.value = colors[idx];                       // カラーアトリビュートの取得
      uint16_t fore = aColors[l.Color.Foreground | (a.Bits.Blink << 3)];
      uint16_t back = aColors[l.Color.Background | (a.Bits.Blink << 3)];
      if (a.Bits.Reverse) swap(fore, back);
      if (mode_ex.Flgs.ScreenReverse) swap(fore, back);
      dt = fontTop[c * CH_H + i];                  // 文字内i行データの取得
      bool prev = (a.Bits.Underline && (i == MAX_CH_Y));
      for (uint16_t j = 0; j < CH_W; j++) {
        bool pset = dt & (0x80 >> j);
        buf[i & 1][cnt] = (pset || prev) ? fore : back;
        if (a.Bits.Bold)
          prev = pset;
        cnt++;
      }
    }
    lcd.pushPixels(buf[i & 1], SP_W, true);
  }
}

// カーソルをホーム位置へ移動
void setCursorToHome() {
  XP = 0;
  YP = 0;
}

// カーソル位置と属性の初期化
void initCursorAndAttribute() {
  cAttr.value = defaultAttr.value;
  cColor.value = defaultColor.value;
  memset(tabs, 0x00, SC_W);
  for (int i = 0; i < SC_W; i += 8)
    tabs[i] = 1;
  setTopAndBottomMargins(1, SC_H);
  mode.value = defaultMode;
  mode_ex.value = defaultModeEx;
}

// 一行スクロール
// (DECSTBM の影響を受ける)
void scroll() {
  if (mode.Flgs.CrLf) XP = 0;
  YP++;
  if (YP > M_BOTTOM) {
    uint16_t n = SCSIZE - SC_W - ((M_TOP + MAX_SC_Y - M_BOTTOM) * SC_W);
    uint16_t idx = SC_W * M_BOTTOM;
    uint16_t idx2;
    uint16_t idx3 = M_TOP * SC_W;
    memmove(&screen[idx3], &screen[idx3 + SC_W], n);
    memmove(&attrib[idx3], &attrib[idx3 + SC_W], n);
    memmove(&colors[idx3], &colors[idx3 + SC_W], n);
    for (int x = 0; x < SC_W; x++) {
      idx2 = idx + x;
      screen[idx2] = 0;
      attrib[idx2] = defaultAttr.value;
      colors[idx2] = defaultColor.value;
    }
    lcd.setAddrWindow(MARGIN_LEFT, M_TOP * CH_H + MARGIN_TOP, SP_W, ((M_BOTTOM + 1) * CH_H) - (M_TOP * CH_H));
    for (int y = M_TOP; y <= M_BOTTOM; y++)
      sc_updateLine(y);
    YP = M_BOTTOM;
  }
}

// パラメータのクリア
void clearParams(em m) {
  escMode = m;
  isDECPrivateMode = false;
  nVals = 0;
  memset(vals, 0, sizeof(vals));
  hasParam = false;
  isNegative = false;
}

// 文字描画
void printChar(char c) {
  // [ESC] キー
  if (c == 0x1b) {
    escMode = em::ES;   // エスケープシーケンス開始
    return;
  }
  // エスケープシーケンス
  if (escMode == em::ES) {
    switch (c) {
      case '[':
        // Control Sequence Introducer (CSI) シーケンス へ
        clearParams(em::CSI);
        break;
      case '#':
        // Line Size Command  シーケンス へ
        clearParams(em::LSC);
        break;
      case '(':
        // G0 セット シーケンス へ
        clearParams(em::G0S);
        break;
      case ')':
        // G1 セット シーケンス へ
        clearParams(em::G1S);
        break;
#ifdef USE_EGR
      case '%':
        // EGR セット シーケンス へ
        clearParams(em::EGR);
        break;
#endif
      default:
        // <ESC> xxx: エスケープシーケンス
        switch (c) {
          case '7':
            // DECSC (Save Cursor): カーソル位置と属性を保存
            saveCursor();
            break;
          case '8':
            // DECRC (Restore Cursor): 保存したカーソル位置と属性を復帰
            restoreCursor();
            break;
          case '=':
            // DECKPAM (Keypad Application Mode): アプリケーションキーパッドモードにセット
            keypadApplicationMode();
            break;
          case '>':
            // DECKPNM (Keypad Numeric Mode): 数値キーパッドモードにセット
            keypadNumericMode();
            break;
          case 'D':
            // IND (Index): カーソルを一行下へ移動
            index(1);
            break;
          case 'E':
            // NEL (Next Line): 改行、カーソルを次行の最初へ移動
            nextLine();
            break;
          case 'H':
            // HTS (Horizontal Tabulation Set): 現在の桁位置にタブストップを設定
            horizontalTabulationSet();
            break;
          case 'M':
            // RI (Reverse Index): カーソルを一行上へ移動
            reverseIndex(1);
            break;
          case 'Z':
            // DECID (Identify): 端末IDシーケンスを送信
            identify();
            break;
          case 'c':
            // RIS (Reset To Initial State): リセット
            resetToInitialState();
            break;
          default:
            // 未確認のシーケンス
            unknownSequence(escMode, c);
            break;
        }
        clearParams(em::NONE);
        break;
    }
    return;
  }

  // "[" Control Sequence Introducer (CSI) シーケンス
  int16_t v1 = 0;
  int16_t v2 = 0;

  if (escMode == em::CSI) {
    escMode = em::CSI2;
    isDECPrivateMode = (c == '?');
    if (isDECPrivateMode) return;
  }

  if (escMode == em::CSI2) {
    if (isdigit(c)) {
      // [パラメータ]
      vals[nVals] = vals[nVals] * 10 + (c - '0');
      hasParam = true;
    } else if (c == ';') {
      // [セパレータ]
      nVals++;
      hasParam = false;
    } else {
      if (hasParam) nVals++;
      switch (c) {
        case 'A':
          // CUU (Cursor Up): カーソルをPl行上へ移動
          v1 = (nVals == 0) ? 1 : vals[0];
          reverseIndex(v1);
          break;
        case 'B':
          // CUD (Cursor Down): カーソルをPl行下へ移動
          v1 = (nVals == 0) ? 1 : vals[0];
          cursorDown(v1);
          break;
        case 'C':
          // CUF (Cursor Forward): カーソルをPc桁右へ移動
          v1 = (nVals == 0) ? 1 : vals[0];
          cursorForward(v1);
          break;
        case 'D':
          // CUB (Cursor Backward): カーソルをPc桁左へ移動
          v1 = (nVals == 0) ? 1 : vals[0];
          cursorBackward(v1);
          break;
        case 'H':
        // CUP (Cursor Position): カーソルをPl行Pc桁へ移動
        case 'f':
          // HVP (Horizontal and Vertical Position): カーソルをPl行Pc桁へ移動
          v1 = (nVals == 0) ? 1 : vals[0];
          v2 = (nVals <= 1) ? 1 : vals[1];
          cursorPosition(v1, v2);
          break;
        case 'J':
          // ED (Erase In Display): 画面を消去
          v1 = (nVals == 0) ? 0 : vals[0];
          eraseInDisplay(v1);
          break;
        case 'K':
          // EL (Erase In Line) 行を消去
          v1 = (nVals == 0) ? 0 : vals[0];
          eraseInLine(v1);
          break;
        case 'L':
          // IL (Insert Line): カーソルのある行の前に Ps 行空行を挿入
          v1 = (nVals == 0) ? 1 : vals[0];
          insertLine(v1);
          break;
        case 'M':
          // DL (Delete Line): カーソルのある行から Ps 行を削除
          v1 = (nVals == 0) ? 1 : vals[0];
          deleteLine(v1);
          break;
        case 'c':
          // DA (Device Attributes): 装置オプションのレポート
          v1 = (nVals == 0) ? 0 : vals[0];
          deviceAttributes(v1);
          break;
        case 'g':
          // TBC (Tabulation Clear): タブストップをクリア
          v1 = (nVals == 0) ? 0 : vals[0];
          tabulationClear(v1);
          break;
        case 'h':
          if (isDECPrivateMode) {
            // DECSET (DEC Set Mode): モードのセット
            decSetMode(vals, nVals);
          } else {
            // SM (Set Mode): モードのセット
            setMode(vals, nVals);
          }
          break;
        case 'l':
          if (isDECPrivateMode) {
            // DECRST (DEC Reset Mode): モードのリセット
            decResetMode(vals, nVals);
          } else {
            // RM (Reset Mode): モードのリセット
            resetMode(vals, nVals);
          }
          break;
        case 'm':
          // SGR (Select Graphic Rendition): 文字修飾の設定
          if (nVals == 0) {
            nVals = 1;
            vals[0] = 0;
          }
          selectGraphicRendition(vals, nVals);
          break;
        case 'n':
          // DSR (Device Status Report): 端末状態のリポート
          v1 = (nVals == 0) ? 0 : vals[0];
          deviceStatusReport(v1);
          break;
        case 'q':
          // DECLL (Load LEDS): LED の設定
          v1 = (nVals == 0) ? 0 : vals[0];
          loadLEDs(v1);
          break;
        case 'r':
          // DECSTBM (Set Top and Bottom Margins): スクロール範囲をPt行からPb行に設定
          v1 = (nVals == 0) ? 1 : vals[0];
          v2 = (nVals <= 1) ? SC_H : vals[1];
          setTopAndBottomMargins(v1, v2);
          break;
        case 'y':
          // DECTST (Invoke Confidence Test): テスト診断を行う
          if ((nVals > 1) && (vals[0] = 2))
            invokeConfidenceTests(vals[1]);
          break;
        default:
          // 未確認のシーケンス
          unknownSequence(escMode, c);
          break;
      }
      clearParams(em::NONE);
    }
    return;
  }

  // "#" Line Size Command  シーケンス
  if (escMode == em::LSC) {
    switch (c) {
      case '3':
        // DECDHL (Double Height Line): カーソル行を倍高、倍幅、トップハーフへ変更
        doubleHeightLine_TopHalf();
        break;
      case '4':
        // DECDHL (Double Height Line): カーソル行を倍高、倍幅、ボトムハーフへ変更
        doubleHeightLine_BotomHalf();
        break;
      case '5':
        // DECSWL (Single-width Line): カーソル行を単高、単幅へ変更
        singleWidthLine();
        break;
      case '6':
        // DECDWL (Double-Width Line): カーソル行を単高、倍幅へ変更
        doubleWidthLine();
        break;
      case '8':
        // DECALN (Screen Alignment Display): 画面を文字‘E’で埋める
        screenAlignmentDisplay();
        break;
      default:
        // 未確認のシーケンス
        unknownSequence(escMode, c);
        break;
    }
    clearParams(em::NONE);
    return;
  }

  // "(" G0 セットシーケンス
  if (escMode == em::G0S) {
    // SCS (Select Character Set): G0 文字コードの設定
    setG0charset(c);
    clearParams(em::NONE);
    return;
  }

  // ")" G1 セットシーケンス
  if (escMode == em::G1S) {
    // SCS (Select Character Set): G1 文字コードの設定
    setG1charset(c);
    clearParams(em::NONE);
    return;
  }

  // "%" EGR シーケンス
#ifdef USE_EGR
  if (escMode == em::EGR) {
    if (isdigit(c) || c == '-') {
      // [パラメータ]
      if (c != '-')
        vals[nVals] = vals[nVals] * 10 + (c - '0');
      else
        isNegative = true;
      hasParam = true;
    } else if (c == ';') {
      // [セパレータ]
      if (isNegative) vals[nVals] = -vals[nVals];
      nVals++;
      hasParam = false;
      isNegative = false;
    } else {
      if (hasParam) {
        if (isNegative) vals[nVals] = -vals[nVals];
        nVals++;
      }
      switch (c) {
        case 'A':
          // drawArc
          lcd.drawArc(vals[0], vals[1], vals[2], vals[3], vals[4], vals[5]);
          break;
        case 'a':
          // fillArc
          lcd.fillArc(vals[0], vals[1], vals[2], vals[3], vals[4], vals[5]);
          break;
        case 'B':
          // drawBezier
          if (nVals == 8)
            lcd.drawBezier(vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7]);
          else
            lcd.drawBezier(vals[0], vals[1], vals[2], vals[3], vals[4], vals[5]);
          break;
        case 'b':
          // playBeep
          switch (nVals) {
            case 2:
              vals[2] = 583;
            case 1:
              vals[1] = 12;
            case 0:
              vals[0] = 1;
          }
          playBeep(vals[0], vals[1], vals[2]);
          break;
        case 'C':
          // drawCircle
          lcd.drawCircle(vals[0], vals[1], vals[2]);
          break;
        case 'c':
          // fillCircle
          lcd.fillCircle(vals[0], vals[1], vals[2]);
          break;
        case 'E':
          // drawEllipse
          lcd.drawEllipse(vals[0], vals[1], vals[2], vals[3]);
          break;
        case 'e':
          // fillEllipse
          lcd.fillEllipse(vals[0], vals[1], vals[2], vals[3]);
          break;
        case 'F':
          // setColor
          lcd.setColor(lcd.color565(vals[0], vals[1], vals[2]));
          break;
        case 'H':
          // drawFastHLine
          lcd.drawFastHLine(vals[0], vals[1], vals[2]);
          break;
        case 'h':
          // カーソル表示/非表示
          textCursorEnableMode((nVals == 0) || (vals[0] == 0));
          break;
        case 'K':
          // setBaseColor
          lcd.setBaseColor(lcd.color565(vals[0], vals[1], vals[2]));
          break;
        case 'L':
          // drawLine
          lcd.drawLine(vals[0], vals[1], vals[2], vals[3]);
          break;
        case 'O':
          // drawRoundRect
          lcd.drawRoundRect(vals[0], vals[1], vals[2], vals[3], vals[4]);
          break;
        case 'o':
          // fillRoundRect
          lcd.fillRoundRect(vals[0], vals[1], vals[2], vals[3], vals[4]);
          break;
        case 'P':
          // drawPixel
          lcd.drawPixel(vals[0], vals[1]);
          break;
        case 'R':
          // drawRect
          lcd.drawRect(vals[0], vals[1], vals[2], vals[3]);
          break;
        case 'r':
          // fillRect
          lcd.fillRect(vals[0], vals[1], vals[2], vals[3]);
          break;
        case 'S':
          // clear
          lcd.clear();
          break;
        case 's':
          // fillScreen
          lcd.fillScreen();
          break;
        case 'T':
          // drawTriangle
          lcd.drawTriangle(vals[0], vals[1], vals[2], vals[3], vals[4], vals[5]);
          break;
        case 't':
          // fillTriangle
          lcd.fillTriangle(vals[0], vals[1], vals[2], vals[3], vals[4], vals[5]);
          break;
        case 'V':
          // drawFastVLine
          lcd.drawFastVLine(vals[0], vals[1], vals[2]);
          break;
        default:
          // 未確認のシーケンス
          unknownSequence(escMode, c);
          break;
      }
      clearParams(em::NONE);
    }
    return;
  }
#endif

  // 改行 (LF / VT / FF)
  if ((c == 0x0a) || (c == 0x0b) || (c == 0x0c)) {
    scroll();
    return;
  }

  // 復帰 (CR)
  if (c == 0x0d) {
    XP = 0;
    return;
  }

  // バックスペース (BS)
  if ((c == 0x08) || (c == 0x7f)) {
    cursorBackward(1);
    uint16_t idx = YP * SC_W + XP;
    screen[idx] = 0;
    attrib[idx] = 0;
    colors[idx] = cColor.value;
    sc_updateChar(XP, YP);
    return;
  }

  // タブ (TAB)
  if (c == 0x09) {
    int16_t idx = -1;
    for (int16_t i = XP + 1; i < SC_W; i++) {
      if (tabs[i]) {
        idx = i;
        break;
      }
    }
    XP = (idx == -1) ? MAX_SC_X : idx;
    return;
  }

  // 通常文字
  if (XP < SC_W) {
    uint16_t idx = YP * SC_W + XP;
    screen[idx] = c;
    attrib[idx] = cAttr.value;
    colors[idx] = cColor.value;
    sc_updateChar(XP, YP);
  }

  // X 位置 + 1
  XP ++;

  // 折り返し行
  if (XP >= SC_W) {
    if (mode_ex.Flgs.WrapLine)
      scroll();
    else
      XP = MAX_SC_X;
  }
}

// 文字列描画
void printString(const char *str) {
  while (*str) printChar(*str++);
}

// エスケープシーケンス
// -----------------------------------------------------------------------------

// DECSC (Save Cursor): カーソル位置と属性を保存
void saveCursor() {
  prev_XP = XP;
  prev_YP = YP;
  bAttr.value = cAttr.value;
  bColor.value = cColor.value;
}

// DECRC (Restore Cursor): 保存したカーソル位置と属性を復帰
void restoreCursor() {
  XP = prev_XP;
  YP = prev_YP;
  cAttr.value = bAttr.value;
  cColor.value = bColor.value;
}

// DECKPAM (Keypad Application Mode): アプリケーションキーパッドモードにセット
void keypadApplicationMode() {
  DebugSerial.println(F("Unimplement: keypadApplicationMode"));
}

// DECKPNM (Keypad Numeric Mode): 数値キーパッドモードにセット
void keypadNumericMode() {
  DebugSerial.println(F("Unimplement: keypadNumericMode"));
}

// IND (Index): カーソルを一行下へ移動
// (DECSTBM の影響を受ける)
void index(int16_t v) {
  cursorDown(v);
}

// NEL (Next Line): 改行、カーソルを次行の最初へ移動
// (DECSTBM の影響を受ける)
void nextLine() {
  scroll();
}

// HTS (Horizontal Tabulation Set): 現在の桁位置にタブストップを設定
void horizontalTabulationSet() {
  tabs[XP] = 1;
}

// RI (Reverse Index): カーソルを一行上へ移動
// (DECSTBM の影響を受ける)
void reverseIndex(int16_t v) {
  cursorUp(v);
}

// DECID (Identify): 端末IDシーケンスを送信
void identify() {
  deviceAttributes(0); // same as DA (Device Attributes)
}

// RIS (Reset To Initial State) リセット
void resetToInitialState() {
  lcd.fillScreen((uint16_t)aColors[defaultColor.Color.Background]);
  initCursorAndAttribute();
  eraseInDisplay(2);
}

// "[" Control Sequence Introducer (CSI) シーケンス
// -----------------------------------------------------------------------------

// CUU (Cursor Up): カーソルをPl行上へ移動
// (DECSTBM の影響を受ける)
void cursorUp(int16_t v) {
  YP -= v;
  if (YP <= M_TOP) YP = M_TOP;
}

// CUD (Cursor Down): カーソルをPl行下へ移動
// (DECSTBM の影響を受ける)
void cursorDown(int16_t v) {
  YP += v;
  if (YP >= M_BOTTOM) YP = M_BOTTOM;
}

// CUF (Cursor Forward): カーソルをPc桁右へ移動
void cursorForward(int16_t v) {
  XP += v;
  if (XP >= SC_W) XP = MAX_SC_X;
}

// CUB (Cursor Backward): カーソルをPc桁左へ移動
void cursorBackward(int16_t v) {
  XP -= v;
  if (XP <= 0) XP = 0;
}

// CUP (Cursor Position): カーソルをPl行Pc桁へ移動
// HVP (Horizontal and Vertical Position): カーソルをPl行Pc桁へ移動
void cursorPosition(uint8_t y, uint8_t x) {
  YP = y - 1;
  if (YP >= SC_H) YP = MAX_SC_Y;
  XP = x - 1;
  if (XP >= SC_W) XP = MAX_SC_X;
}

// 画面を再描画
void refreshScreen() {
  lcd.setAddrWindow(MARGIN_LEFT, MARGIN_TOP, SP_W, SP_H);
  for (int i = 0; i < SC_H; i++)
  for (uint8_t i = 0; i < SC_H; i++)
    sc_updateLine(i);
}

// ED (Erase In Display): 画面を消去
void eraseInDisplay(uint8_t m) {
  uint8_t sl = 0, el = 0;
  uint16_t idx = 0, n = 0;

  switch (m) {
    case 0:
      // カーソルから画面の終わりまでを消去
      sl = YP;
      el = MAX_SC_Y;
      idx = YP * SC_W + XP;
      n   = SCSIZE - (YP * SC_W + XP);
      break;
    case 1:
      // 画面の始めからカーソルまでを消去
      sl = 0;
      el = YP;
      idx = 0;
      n = YP * SC_W + XP + 1;
      break;
    case 2:
      // 画面全体を消去
      sl = 0;
      el = MAX_SC_Y;
      idx = 0;
      n = SCSIZE;
      break;
  }

  if (m <= 2) {
    memset(&screen[idx], 0x00, n);
    memset(&attrib[idx], defaultAttr.value, n);
    memset(&colors[idx], defaultColor.value, n);
    if (m == 2)
      lcd.clear(aColors[defaultColor.Color.Background]);
    else {
      lcd.setAddrWindow(MARGIN_LEFT, sl * CH_H + MARGIN_TOP, SP_W, ((el + 1) * CH_H) - (sl * CH_H));
      for (int i = sl; i <= el; i++)
        sc_updateLine(i);
    }
  }
}

// EL (Erase In Line): 行を消去
void eraseInLine(uint8_t m) {
  uint16_t slp = 0, elp = 0;

  switch (m) {
    case 0:
      // カーソルから行の終わりまでを消去
      slp = YP * SC_W + XP;
      elp = YP * SC_W + MAX_SC_X;
      break;
    case 1:
      // 行の始めからカーソルまでを消去
      slp = YP * SC_W;
      elp = YP * SC_W + XP;
      break;
    case 2:
      // 行全体を消去
      slp = YP * SC_W;
      elp = YP * SC_W + MAX_SC_X;
      break;
  }

  if (m <= 2) {
    uint16_t n = elp - slp + 1;
    memset(&screen[slp], 0x00, n);
    memset(&attrib[slp], defaultAttr.value, n);
    memset(&colors[slp], defaultColor.value, n);
    lcd.setAddrWindow(MARGIN_LEFT, YP * CH_H + MARGIN_TOP, SP_W, ((YP + 1) * CH_H) - (YP * CH_H));
    sc_updateLine(YP);
  }
}

// IL (Insert Line): カーソルのある行の前に Ps 行空行を挿入
// (DECSTBM の影響を受ける)
void insertLine(uint8_t v) {
  int16_t rows = v;
  if (rows == 0) return;
  if (rows > ((M_BOTTOM + 1) - YP)) rows = (M_BOTTOM + 1) - YP;
  int16_t idx = SC_W * YP;
  int16_t n = SC_W * rows;
  int16_t idx2 = idx + n;
  int16_t move_rows = (M_BOTTOM + 1) - YP - rows;
  int16_t n2 = SC_W * move_rows;

  if (move_rows > 0) {
    memmove(&screen[idx2], &screen[idx], n2);
    memmove(&attrib[idx2], &attrib[idx], n2);
    memmove(&colors[idx2], &colors[idx], n2);
  }
  memset(&screen[idx], 0x00, n);
  memset(&attrib[idx], defaultAttr.value, n);
  memset(&colors[idx], defaultColor.value, n);
  lcd.setAddrWindow(MARGIN_LEFT, YP * CH_H + MARGIN_TOP, SP_W, ((M_BOTTOM + 1) * CH_H) - (YP * CH_H));
  for (int y = YP; y <= M_BOTTOM; y++)
    sc_updateLine(y);
}

// DL (Delete Line): カーソルのある行から Ps 行を削除
// (DECSTBM の影響を受ける)
void deleteLine(uint8_t v) {
  int16_t rows = v;
  if (rows == 0) return;
  if (rows > ((M_BOTTOM + 1) - YP)) rows = (M_BOTTOM + 1) - YP;
  int16_t idx = SC_W * YP;
  int16_t n = SC_W * rows;
  int16_t idx2 = idx + n;
  int16_t move_rows = (M_BOTTOM + 1) - YP - rows;
  int16_t n2 = SC_W * move_rows;
  int16_t idx3 = (M_BOTTOM + 1) * SC_W - n;

  if (move_rows > 0) {
    memmove(&screen[idx], &screen[idx2], n2);
    memmove(&attrib[idx], &attrib[idx2], n2);
    memmove(&colors[idx], &colors[idx2], n2);
  }
  memset(&screen[idx3], 0x00, n);
  memset(&attrib[idx3], defaultAttr.value, n);
  memset(&colors[idx3], defaultColor.value, n);
  memset(&colors[idx3], defaultColor.value, n);
  for (int y = YP; y <= M_BOTTOM; y++)
    sc_updateLine(y);
}

// CPR (Cursor Position Report): カーソル位置のレポート
void cursorPositionReport(uint16_t y, uint16_t x) {
  // Need Debug
  printString("\e[");
  printString(String(y, DEC).c_str());
  printString(";");
  printString(String(x, DEC).c_str());
  printString("R"); // CPR (Cursor Position Report)
}

// DA (Device Attributes): 装置オプションのレポート
// オプションのレポート
void deviceAttributes(uint8_t m) {
  printString("\e[?1;0c"); // Need Debug
}

// TBC (Tabulation Clear): タブストップをクリア
void tabulationClear(uint8_t m) {
  switch (m) {
    case 0:
      // 現在位置のタブストップをクリア
      tabs[XP] = 0;
      break;
    case 3:
      // すべてのタブストップをクリア
      memset(tabs, 0x00, SC_W);
      break;
  }
}

// LNM (Line Feed / New Line Mode): 改行モード
void lineMode(bool m) {
  mode.Flgs.CrLf = m;
}

// DECSCNM (Screen Mode): 画面反転モード
void screenMode(bool m) {
  mode_ex.Flgs.ScreenReverse = m;
  refreshScreen();
}

// DECAWM (Auto Wrap Mode): 自動折り返しモード
void autoWrapMode(bool m) {
  mode_ex.Flgs.WrapLine = m;
}

// DECTCEM (Text Cursor Enable Mode): テキストカーソル有効モード
void textCursorEnableMode(bool m) {
  hideCursor = !m;
  if (hideCursor) {
    sc_updateChar(p_XP, p_YP);
    p_XP = XP;
    p_YP = YP;
  }
}

// SM (Set Mode): モードのセット
void setMode(int16_t *vals, int16_t nVals) {
  for (int16_t i = 0; i < nVals; i++) {
    switch (vals[i]) {
      case 20:
        // LNM (Line Feed / New Line Mode)
        lineMode(true);
        break;
      default:
        DebugSerial.print(F("Unimplement: setMode "));
        DebugSerial.println(String(vals[i], DEC));
        break;
    }
  }
}

// DECSET (DEC Set Mode): モードのセット
void decSetMode(int16_t *vals, int16_t nVals) {
  for (int16_t i = 0; i < nVals; i++) {
    switch (vals[i]) {
      case 5:
        // DECSCNM (Screen Mode): 画面反転モード
        screenMode(true);
        break;
      case 7:
        // DECAWM (Auto Wrap Mode): 自動折り返しモード
        autoWrapMode(true);
        break;
      case 25:
        // DECTCEM (Text Cursor Enable Mode): テキストカーソル有効モード
        textCursorEnableMode(true);
        break;
      default:
        DebugSerial.print(F("Unimplement: decSetMode "));
        DebugSerial.println(String(vals[i], DEC));
        break;
    }
  }
}

// RM (Reset Mode): モードのリセット
void resetMode(int16_t *vals, int16_t nVals) {
  for (int16_t i = 0; i < nVals; i++) {
    switch (vals[i]) {
      case 20:
        // LNM (Line Feed / New Line Mode)
        lineMode(false);
        break;
      default:
        DebugSerial.print(F("Unimplement: resetMode "));
        DebugSerial.println(String(vals[i], DEC));
        break;
    }
  }
}

// DECRST (DEC Reset Mode): モードのリセット
void decResetMode(int16_t *vals, int16_t nVals) {
  for (int16_t i = 0; i < nVals; i++) {
    switch (vals[i]) {
      case 5:
        // DECSCNM (Screen Mode): 画面反転モード
        screenMode(false);
        break;
      case 7:
        // DECAWM (Auto Wrap Mode): 自動折り返しモード
        autoWrapMode(false);
        break;
      case 25:
        // DECTCEM (Text Cursor Enable Mode): テキストカーソル有効モード
        textCursorEnableMode(false);
        break;
      default:
        DebugSerial.print(F("Unimplement: decResetMode "));
        DebugSerial.println(String(vals[i], DEC));
        break;
    }
  }
}

// SGR (Select Graphic Rendition): 文字修飾の設定
void selectGraphicRendition(int16_t *vals, int16_t nVals) {
  uint8_t seq = 0;
  uint16_t r, g, b, cIdx;
  bool isFore = true;
  for (int16_t i = 0; i < nVals; i++) {
    int16_t v = vals[i];
    switch (seq) {
      case 0:
        switch (v) {
          case 0:
            // 属性クリア
            cAttr.value = 0;
            cColor.value = defaultColor.value;
            break;
          case 1:
            // 太字
            cAttr.Bits.Bold = 1;
            break;
          case 4:
            // アンダーライン
            cAttr.Bits.Underline = 1;
            break;
          case 5:
            // 点滅 (明色表現)
            cAttr.Bits.Blink = 1;
            break;
          case 7:
            // 反転
            cAttr.Bits.Reverse = 1;
            break;
          case 21:
            // 二重下線 or 太字オフ
            cAttr.Bits.Bold = 0;
            break;
          case 22:
            // 太字オフ
            cAttr.Bits.Bold = 0;
            break;
          case 24:
            // アンダーラインオフ
            cAttr.Bits.Underline = 0;
            break;
          case 25:
            // 点滅 (明色表現) オフ
            cAttr.Bits.Blink = 0;
            break;
          case 27:
            // 反転オフ
            cAttr.Bits.Reverse = 0;
            break;
          case 38:
            seq = 1;
            isFore = true;
            break;
          case 39:
            // 前景色をデフォルトに戻す
            cColor.Color.Foreground = defaultColor.Color.Foreground;
            break;
          case 48:
            seq = 1;
            isFore = false;
            break;
          case 49:
            // 背景色をデフォルトに戻す
            cColor.Color.Background = defaultColor.Color.Background;
            break;
          default:
            if (v >= 30 && v <= 37) {
              // 前景色
              cColor.Color.Foreground = v - 30;
            } else if (v >= 40 && v <= 47) {
              // 背景色
              cColor.Color.Background = v - 40;
            }
            break;
        }
        break;
      case 1:
        switch (v) {
          case 2:
            // RGB
            seq = 3;
            break;
          case 5:
            // Color Index
            seq = 2;
            break;
          default:
            seq = 0;
            break;
        }
        break;
      case 2:
        // Index Color
        if (v < 256) {
          if (v < 16) {
            // ANSI カラー (16 色のインデックスカラーが使われる)
            cIdx = v;
          } else if (v < 232) {
            // 6x6x6 RGB カラー (8 色のインデックスカラー中で最も近い色が使われる)
            b = ( (v - 16)       % 6) / 3;
            g = (((v - 16) /  6) % 6) / 3;
            r = (((v - 16) / 36) % 6) / 3;
            cIdx = (b << 2) | (g << 1) | r;
          } else {
            // 24 色グレースケールカラー (2 色のグレースケールカラーが使われる)
            if (v < 244)
              cIdx = clBlack;
            else
              cIdx = clWhite;
          }
          if (isFore)
            cColor.Color.Foreground = cIdx;
          else
            cColor.Color.Background = cIdx;
        }
        seq = 0;
        break;
      case 3:
        // RGB - R
        seq = 4;
        break;
      case 4:
        // RGB - G
        seq = 5;
        break;
      case 5:
        // RGB - B
        // RGB (8 色のインデックスカラー中で最も近い色が使われる)
        r = map(vals[i - 2], 0, 255, 0, 1);
        g = map(vals[i - 1], 0, 255, 0, 1);
        b = map(vals[i - 0], 0, 255, 0, 1);
        cIdx = (b << 2) | (g << 1) | r;
        if (isFore)
          cColor.Color.Foreground = cIdx;
        else
          cColor.Color.Background = cIdx;
        seq = 0;
        break;
      default:
        seq = 0;
        break;
    }
  }
}

// DSR (Device Status Report): 端末状態のリポート
void deviceStatusReport(uint8_t m) {
  switch (m) {
    case 5:
      printString("\e[0n"); // Need Debug
      break;
    case 6:
      cursorPositionReport(XP, YP); // CPR (Cursor Position Report)
      break;
  }
}

// DECLL (Load LEDS): LED の設定
void loadLEDs(uint8_t m) {
  switch (m) {
    case 0:
      // すべての LED をオフ
/*
      digitalWrite(LED_01, LOW);
      digitalWrite(LED_02, LOW);
      digitalWrite(LED_03, LOW);
      digitalWrite(LED_04, LOW);
*/      
      break;
    case 1:
      // LED1 をオン
//      digitalWrite(LED_01, HIGH);
      break;
    case 2:
      // LED2 をオン
//      digitalWrite(LED_02, HIGH);
      break;
    case 3:
      // LED3 をオン
//      digitalWrite(LED_03, HIGH);
      break;
    case 4:
      // LED4 をオン
//      digitalWrite(LED_04, HIGH);
      break;
  }
}

// DECSTBM (Set Top and Bottom Margins): スクロール範囲をPt行からPb行に設定
void setTopAndBottomMargins(int16_t s, int16_t e) {
  if (e <= s) return;
  M_TOP    = s - 1;
  if (M_TOP < 0) M_TOP = 0;
  if (M_TOP > MAX_SC_Y) M_TOP = MAX_SC_Y;
  M_BOTTOM = e - 1;
  if (M_BOTTOM < 0) M_BOTTOM = 0;
  if (M_BOTTOM > MAX_SC_Y) M_BOTTOM = MAX_SC_Y;
  setCursorToHome();
}

// DECTST (Invoke Confidence Test): テスト診断を行う
void invokeConfidenceTests(uint8_t m) {
  NVIC_SystemReset();
}

// "]" Operating System Command (OSC) シーケンス
// -----------------------------------------------------------------------------

// "#" Line Size Command  シーケンス
// -----------------------------------------------------------------------------

// DECDHL (Double Height Line): カーソル行を倍高、倍幅、トップハーフへ変更
void doubleHeightLine_TopHalf() {
  DebugSerial.println(F("Unimplement: doubleHeightLine_TopHalf"));
}

// DECDHL (Double Height Line): カーソル行を倍高、倍幅、ボトムハーフへ変更
void doubleHeightLine_BotomHalf() {
  DebugSerial.println(F("Unimplement: doubleHeightLine_BotomHalf"));
}

// DECSWL (Single-width Line): カーソル行を単高、単幅へ変更
void singleWidthLine() {
  DebugSerial.println(F("Unimplement: singleWidthLine"));
}

// DECDWL (Double-Width Line): カーソル行を単高、倍幅へ変更
void doubleWidthLine() {
  DebugSerial.println(F("Unimplement: doubleWidthLine"));
}

// DECALN (Screen Alignment Display): 画面を文字‘E’で埋める
void screenAlignmentDisplay() {
  memset(screen, 0x45, SCSIZE);
  memset(attrib, defaultAttr.value, SCSIZE);
  memset(colors, defaultColor.value, SCSIZE);
  lcd.setAddrWindow(MARGIN_LEFT, MARGIN_TOP, SP_W, SP_H);
  for (int y = 0; y < SC_H; y++)
    sc_updateLine(y);
}

// "(" G0 Sets Sequence
// -----------------------------------------------------------------------------

// G0 文字コードの設定
void setG0charset(char c) {
  DebugSerial.println(F("Unimplement: setG0charset"));
}

// "(" G1 Sets Sequence
// -----------------------------------------------------------------------------

// G1 文字コードの設定
void setG1charset(char c) {
  DebugSerial.println(F("Unimplement: setG1charset"));
}

// Unknown Sequence
// -----------------------------------------------------------------------------

// 不明なシーケンス
void unknownSequence(em m, char c) {
  String s = (m != em::NONE) ? "[ESC]" : "";
  switch (m) {
    case em::CSI:
      s = s + " [";
      break;
    case em::LSC:
      s = s + " #";
      break;
    case em::G0S:
      s = s + " (";
      break;
    case em::G1S:
      s = s + " )";
      break;
    case em::CSI2:
      s = s + " [";
      if (isDECPrivateMode) 
        s = s + "?";
      break;
    case em::EGR:
      s = s + " %";
      break;
  }
  DebugSerial.print(F("Unknown: "));
  DebugSerial.print(s);
  DebugSerial.print(F(" "));
  DebugSerial.print(c);
}

// -----------------------------------------------------------------------------

// タイマーハンドラ
void handle_timer() {
  canShowCursor = true;
}

// Play Tone
void playTone(int Pin, int Tone, int Duration) {
  for (long i = 0; i < Duration * 1000L; i += Tone * 2) {
    digitalWrite(Pin, HIGH);
    delayMicroseconds(Tone);
    digitalWrite(Pin, LOW);
    delayMicroseconds(Tone);
  }
}

// Play Beep
void playBeep(const uint16_t Number, const uint8_t ToneNo, const uint16_t Duration) {
  double freq = ((ToneNo == 12) && (Duration == 583)) ? 4000 : 256000.0 / (90 + 4 * ToneNo);
  if (freq <  230.6) freq =  230.6;
  if (freq > 2844.4) freq = 2844.4;
  double timeHigh = 1000000L / (2 * freq);
  for (uint16_t i = 0; i < Number; i++) {
    playTone(SPK_PIN, timeHigh, Duration);
    if (i < (Number - 1)) delay(300);
  }
  delay(20);
}

// セットアップ
void setup() {
  Wire.begin( );    // Define(SDA, SCL)
  DebugSerial.begin(115200);
  delay(500);

  // LED の初期化
/*
  pinMode(LED_01, OUTPUT);
  pinMode(LED_02, OUTPUT);
  pinMode(LED_03, OUTPUT);
  pinMode(LED_04, OUTPUT);
  digitalWrite(LED_01, LOW);
  digitalWrite(LED_02, LOW);
  digitalWrite(LED_03, LOW);
  digitalWrite(LED_04, LOW);
*/
  
  // LCD の初期化
  lcd.init();
  lcd.startWrite();
  lcd.setRotation(1);
  lcd.setColorDepth(16);
  
  fontTop = (uint8_t*)font6x8tt + 3;
  resetToInitialState();
  printString("\e[0;44m *** Terminal Init *** \e[0m\n");
  setCursorToHome();

  // カーソル用タイマーの設定
  TC.startTimer(200000, handle_timer); // 200ms

  // スイッチの初期化
  for (int i = 0; i < 5; i++)
    pinMode(SW_PORT[i], INPUT_PULLUP);

  // ボタンの初期化
  for (int i = 0; i < 3; i++)
    pinMode(BTN_PORT[i], INPUT_PULLUP);

  // ブザーの初期化
  pinMode(SPK_PIN, OUTPUT);  

//---------RunCPM-----------------------------------------------------

#ifdef DEBUGLOG
  _sys_deletefile((uint8 *)LogName);
#endif

  _clrscr();
  _puts("CP/M 2.2 Emulator v" VERSION " by Marcelo Dantas\r\n");
  _puts("Arduino read/write support by Krzysztof Klis\r\n");
  _puts("      Build " __DATE__ " - " __TIME__ "\r\n");
  _puts("--------------------------------------------\r\n");
  _puts("CCP: " CCPname "    CCP Address: 0x");
  _puthex16(CCPaddr);
  _puts("\r\nBOARD: ");
  _puts(BOARD);
  _puts("\r\n");

  _puts("Initializing SD card.\r\n");
  if (SD.begin(SDINIT, SD_SCK_MHZ(50))) {
    if (VersionCCP >= 0x10 || SD.exists(CCPname)) {
      while (true) {
        _puts(CCPHEAD);
        _PatchCPM();
        Status = 0;
#ifndef CCP_INTERNAL
        if (!_RamLoad((char *)CCPname, CCPaddr)) {
          _puts("Unable to load the CCP.\r\nCPU halted.\r\n");
          break;
        }
        Z80reset();
        SET_LOW_REGISTER(BC, _RamRead(0x0004));
        PC = CCPaddr;
        Z80run();
#else
        _ccp();
#endif
        if (Status == 1)
          break;
#ifdef USE_PUN
        if (pun_dev)
          _sys_fflush(pun_dev);
#endif
#ifdef USE_LST
        if (lst_dev)
          _sys_fflush(lst_dev);
#endif
      }
    } else {
      _puts("Unable to load CP/M CCP.\r\nCPU halted.\r\n");
    }
  } else {
    _puts("Unable to initialize SD card.\r\nCPU halted.\r\n");
  }
}

// ループ
void loop() {
}
