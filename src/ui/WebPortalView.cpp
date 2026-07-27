#include "ui/WebPortalView.h"

#if M5EPUB_ENABLE_WEB_PORTAL
#include "UiStrings.h"
#include "layout/PortugueseTextRenderer.h"
#include "layout/ReaderFont.h"
#include <algorithm>

namespace {
constexpr int32_t kNetworkTop = 130;
constexpr int32_t kNetworkHeight = 82;
constexpr size_t kVisibleNetworks = 7;
constexpr char kKeys[] = "1234567890QWERTYUIOPASDFGHJKL_ZXCVBNM-";
}

void WebPortalView::renderNetworks(const WifiService& wifi) {
  Serial.printf("M5EPUB_WIFI,event=render_networks,state=%u,visible_count=%u\n",
                static_cast<unsigned>(wifi.state()),
                static_cast<unsigned>(wifi.networks().size()));
  M5Canvas& canvas = display_.canvas();
  canvas.fillScreen(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setFont(reader_font::forSize(24, ReaderFontFamily::Compact));
  canvas.setTextDatum(top_left);
  const bool pt = ui_strings::language() == UiLanguage::Portuguese;
  const std::string title = pt ? "Redes Wi-Fi" : "Wi-Fi networks";
  portuguese_text::draw(canvas, title,
      (display_.width() - portuguese_text::width(canvas, title)) / 2, 30);
  canvas.drawFastHLine(24, 88, display_.width() - 48, TFT_BLACK);
  canvas.setFont(reader_font::forSize(16, ReaderFontFamily::Compact));
  if (wifi.state() == WifiState::Scanning) {
    const std::string scanning = pt ? "Procurando redes..." : "Scanning networks...";
    portuguese_text::draw(canvas, scanning, 34, 145);
  } else if (wifi.state() == WifiState::Failed) {
    const std::string failed = pt ? "Não foi possível buscar redes:" :
                                    "Could not scan networks:";
    portuguese_text::draw(canvas, failed, 34, 145);
    portuguese_text::draw(canvas, wifi.lastError(), 34, 190);
    const std::string retry = pt ? "Toque em Buscar novamente." :
                                   "Tap Scan again.";
    portuguese_text::draw(canvas, retry, 34, 235);
  } else {
    const size_t count = std::min(wifi.networks().size(), kVisibleNetworks);
    for (size_t index = 0; index < count; ++index) {
      const int32_t top = kNetworkTop + static_cast<int32_t>(index) * kNetworkHeight;
      canvas.drawRect(24, top, display_.width() - 48, kNetworkHeight - 8, TFT_BLACK);
      std::string label = wifi.networks()[index].ssid;
      if (wifi.networks()[index].saved) label += pt ? "  [salva]" : "  [saved]";
      portuguese_text::draw(canvas, label, 40, top + 18);
      char signal[24];
      snprintf(signal, sizeof(signal), "%ld dBm%s",
               static_cast<long>(wifi.networks()[index].rssi),
               wifi.networks()[index].secured ? "  *" : "");
      canvas.drawString(signal, display_.width() - 145, top + 42);
    }
  }
  canvas.drawRect(24, 730, 230, 74, TFT_BLACK);
  canvas.drawRect(286, 730, 230, 74, TFT_BLACK);
  portuguese_text::draw(canvas, pt ? "Buscar novamente" : "Scan again", 42, 752);
  portuguese_text::draw(canvas, pt ? "Esquecer redes" : "Forget networks", 300, 752);
  canvas.drawRect(24, 835, display_.width() - 48, 72, TFT_BLACK);
  portuguese_text::draw(canvas, pt ? "Voltar" : "Back", 235, 855);
  display_.submitFull(RefreshIntent::FullQuality);
}

void WebPortalView::renderPassword(const std::string& ssid,
                                   const std::string& password,
                                   bool remember, const std::string& error) {
  M5Canvas& canvas = display_.canvas();
  canvas.fillScreen(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setFont(reader_font::forSize(24, ReaderFontFamily::Compact));
  canvas.setTextDatum(top_left);
  const bool pt = ui_strings::language() == UiLanguage::Portuguese;
  portuguese_text::draw(canvas, pt ? "Senha do Wi-Fi" : "Wi-Fi password", 32, 24);
  canvas.setFont(reader_font::forSize(16, ReaderFontFamily::Compact));
  portuguese_text::draw(canvas, ssid, 32, 78);
  canvas.drawRect(24, 120, display_.width() - 48, 65, TFT_BLACK);
  std::string masked(password.size(), '*');
  if (masked.size() > 30) masked = masked.substr(masked.size() - 30);
  canvas.drawString(masked.c_str(), 38, 143);
  if (!error.empty()) portuguese_text::draw(canvas, error, 30, 195);
  constexpr int columns = 10;
  constexpr int32_t keyWidth = 52;
  constexpr int32_t keyHeight = 74;
  constexpr int32_t keyTop = 235;
  for (size_t index = 0; index < sizeof(kKeys) - 1; ++index) {
    const int32_t x = 10 + static_cast<int32_t>(index % columns) * keyWidth;
    const int32_t y = keyTop + static_cast<int32_t>(index / columns) * keyHeight;
    canvas.drawRect(x, y, keyWidth - 4, keyHeight - 6, TFT_BLACK);
    char key[2] = {kKeys[index] == '_' ? ' ' : kKeys[index], 0};
    canvas.setTextDatum(middle_center);
    canvas.drawString(key, x + (keyWidth - 4) / 2, y + (keyHeight - 6) / 2);
  }
  canvas.setTextDatum(top_left);
  canvas.drawRect(24, 555, 230, 67, TFT_BLACK);
  canvas.drawRect(286, 555, 230, 67, TFT_BLACK);
  portuguese_text::draw(canvas, pt ? "Apagar" : "Backspace", 70, 575);
  portuguese_text::draw(canvas, remember ? "[x] Salvar" : "[ ] Salvar", 330, 575);
  canvas.drawRect(24, 680, display_.width() - 48, 76, TFT_BLACK);
  portuguese_text::draw(canvas, pt ? "Conectar" : "Connect", 220, 705);
  canvas.drawRect(24, 820, display_.width() - 48, 70, TFT_BLACK);
  portuguese_text::draw(canvas, pt ? "Cancelar" : "Cancel", 220, 840);
  display_.submitFull(RefreshIntent::FullQuality);
}

WebPortalViewHit WebPortalView::hitNetworks(int32_t x, int32_t y,
                                            size_t networkCount) const {
  WebPortalViewHit hit;
  if (y >= kNetworkTop && y < kNetworkTop + static_cast<int32_t>(kVisibleNetworks) * kNetworkHeight) {
    const size_t index = static_cast<size_t>((y - kNetworkTop) / kNetworkHeight);
    if (index < networkCount) { hit.action = WebPortalViewAction::Network; hit.networkIndex = index; }
  } else if (y >= 730 && y < 804) hit.action = x < 270 ? WebPortalViewAction::Rescan : WebPortalViewAction::ForgetAll;
  else if (y >= 835 && y < 907) hit.action = WebPortalViewAction::Cancel;
  return hit;
}

WebPortalViewHit WebPortalView::hitPassword(int32_t x, int32_t y) const {
  WebPortalViewHit hit;
  if (y >= 235 && y < 531 && x >= 10 && x < 530) {
    const size_t index = static_cast<size_t>((y - 235) / 74) * 10 +
                         static_cast<size_t>((x - 10) / 52);
    if (index < sizeof(kKeys) - 1) {
      hit.action = WebPortalViewAction::Key;
      hit.key = kKeys[index] == '_' ? ' ' : kKeys[index];
    }
  } else if (y >= 555 && y < 622) hit.action = x < 270 ? WebPortalViewAction::Backspace : WebPortalViewAction::ToggleRemember;
  else if (y >= 680 && y < 756) hit.action = WebPortalViewAction::Connect;
  else if (y >= 820 && y < 890) hit.action = WebPortalViewAction::Cancel;
  return hit;
}
#endif
