
#include <Arduino.h>
#include <ArduinoJson.h>
#include <GxEPD2_BW.h>  // <-- FIX: Include main library first
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <map>
#include <ctime>

#include "gang.xbm"
#include "nam.xbm"
#include "go.xbm"
#include "sok.xbm"
#include "to.xbm"
#include "mi.xbm"
#include "nol.xbm"
#include "seo.xbm"
#include "cho.xbm"
#include "bu.xbm"
#include "ho.xbm"
#include "son.xbm"
// #include "num2.xbm"
// #include "num3.xbm"

#include <WiFiManager.h>

#define GxEPD2_BUS_CLASS SPI

#define EPD_CS 5
#define EPD_DC 17
#define EPD_RST 16
#define EPD_BUSY 4
#define LED_BUILTIN 13

#define SERIAL_BAUD 115200

struct Glyph {
  const uint8_t* data;
  uint16_t w;
  uint16_t h;
};

std::map<std::string, Glyph> glyphs;

void init_glyphs() {
  glyphs["강"] = {gang_bits, gang_width, gang_height};
  glyphs["남"] = {nam_bits, nam_width, nam_height};
  glyphs["고"] = {go_bits, go_width, go_height};
  glyphs["석"] = {sok_bits, sok_width, sok_height};
  glyphs["터"] = {to_bits, to_width, to_height};
  glyphs["미"] = {mi_bits, mi_width, mi_height};
  glyphs["널"] = {nol_bits, nol_width, nol_height};
  glyphs["서"] = {seo_bits, seo_width, seo_height};
  glyphs["초"] = {cho_bits, cho_width, cho_height};
  glyphs["부"] = {bu_bits, bu_width, bu_height};
  glyphs["호"] = {ho_bits, ho_width, ho_height};
  glyphs["선"] = {son_bits, son_width, son_height};
  // glyphs["2"] = {num2_bits, num2_width, num2_height};
  // glyphs["3"] = {num3_bits, num3_width, num3_height};
}

GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420::HEIGHT> display =
    GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420::HEIGHT>(
        GxEPD2_420_GDEY042T81(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

const char* LINE_KEY = "updnLine";
const char* ARRIVAL_TIME_KEY = "barvlDt";
const char* API_URL =
    "http://swopenAPI.seoul.go.kr/api/subway/65794d52696a736335374c4a5a476a/"
    "json/realtimeStationArrival/0/10/%EA%B5%90%EB%8C%80";

constexpr size_t MAX_ARRIVALS = 8;
constexpr size_t JSON_DOC_SIZE = 6144;
constexpr int32_t LOOP_DELAY_SECONDS = 20;
constexpr size_t LONGEST_STATION_NAME_LENGTH = 5;

void drawHangul(GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420::HEIGHT>& display,
                int x, int yTop, const String& text) {
  int cursor = x;

  for (size_t i = 0; i < text.length();) {
    uint8_t lead = text[i];
    int char_len = 1;
    if ((lead & 0x80) == 0x00)
      char_len = 1;
    else if ((lead & 0xE0) == 0xC0)
      char_len = 2;
    else if ((lead & 0xF0) == 0xE0)
      char_len = 3;
    else if ((lead & 0xF8) == 0xF0)
      char_len = 4;

    String ch = text.substring(i, i + char_len);
    i += char_len;

    if (ch == " ") {
      cursor += 10;  // space gap
      continue;
    }
    std::string str_ch = std::string(ch.c_str());

    auto it = glyphs.find(str_ch);

    if (it == glyphs.end()) {
      // glyph not available: skip or add spacing
      cursor += 8;
      continue;
    }

    const Glyph& g = it->second;

    // Draw using top-left origin (yTop is top)
    if (strcmp(str_ch.c_str(), "초") == 0) {
      yTop += 4;  // adjust for "초" as it is kind of stubby.
    }
    display.drawXBitmap(cursor, yTop, g.data, g.w, g.h, GxEPD_BLACK);

    cursor += g.w + 2;
  }
}

void print_all() { Serial.println(); }

template <typename T, typename... Args>
void print_all(T first_arg, Args... other_args) {
  Serial.print(first_arg);
  Serial.print(" ");  // Add a separator
  print_all(other_args...);
}

struct ArrivalInfo {
  uint8_t line_number;
  int32_t arrival_time;
  const char* next_station_name;
};

struct ArrivalResult {
  ArrivalInfo* data;
  size_t count;
};

bool get_line_info(const char* line_key, uint8_t& line_number,
                   const char*& next_station) {
  if (strcmp(line_key, "외선") == 0) {
    line_number = 2;
    next_station = "강남";
  } else if (strcmp(line_key, "하행") == 0) {
    line_number = 3;
    next_station = "남부";
  } else if (strcmp(line_key, "상행") == 0) {
    line_number = 3;
    next_station = "고터";
  } else if (strcmp(line_key, "내선") == 0) {
    line_number = 2;
    next_station = "서초";
  } else {
    return false;  // Unknown line key
  }
  return true;  // Success
}

bool get_arrival_info_json(JsonDocument& arrivals_json_data) {
  if (WiFi.status() != WL_CONNECTED) {
    print_all("WiFi not connected!");
    return false;
  }
  HTTPClient http;
  http.begin(API_URL);
  int http_code = http.GET();
  if (http_code != HTTP_CODE_OK) {
    print_all("HTTP GET failed, error:", http_code);
    http.end();
    return false;
  }
  String payload = http.getString();
  DeserializationError error = deserializeJson(arrivals_json_data, payload);
  if (error) {
    print_all("JSON deserialization failed:", error.c_str());
    http.end();
    return false;
  }
  http.end();
  return true;
}

ArrivalResult parse_arrival_info_json(JsonDocument& arrivals_json_data) {
  JsonArray data = arrivals_json_data["realtimeArrivalList"].as<JsonArray>();
  static ArrivalInfo arrival_info_array[MAX_ARRIVALS];
  size_t total_arrivals = 0;
  for (size_t i = 0; i < MAX_ARRIVALS; ++i) {
    if (i >= data.size()) {
      break;
    }
    JsonObject item = data[i];
    const char* train_line = item[LINE_KEY];
    const char* arrival_time = item[ARRIVAL_TIME_KEY];
    const char* next_station;
    print_all("Debug:", train_line, arrival_time);
    uint8_t line_number = 0;
    if (!get_line_info(train_line, line_number, next_station)) {
      print_all("Unknown line key:", train_line);
      continue;
    }
    const ArrivalInfo arrival_info =
        ArrivalInfo{line_number, atoi(arrival_time), next_station};
    arrival_info_array[i] = arrival_info;
    ++total_arrivals;
  }
  return ArrivalResult{arrival_info_array, total_arrivals};
}

void sort_arrivals_by_time(ArrivalResult& arrival_result) {
  for (size_t i = 0; i < arrival_result.count; ++i) {
    for (size_t j = i + 1; j < arrival_result.count; ++j) {
      if (arrival_result.data[i].arrival_time >
          arrival_result.data[j].arrival_time) {
        std::swap(arrival_result.data[i], arrival_result.data[j]);
      }
    }
  }
}

ArrivalResult fetch_and_sort_arrivals_for_station() {
  static StaticJsonDocument<JSON_DOC_SIZE> arrival_info_json;
  if (!get_arrival_info_json(arrival_info_json)) {
    arrival_info_json.clear();
    return ArrivalResult{nullptr, 0};
  }
  ArrivalResult arrival_result = parse_arrival_info_json(arrival_info_json);
  arrival_info_json.clear();
  sort_arrivals_by_time(arrival_result);
  return arrival_result;
}

void format_and_print_arrivals(const ArrivalResult& arrival_result) {
  for (size_t i = 0; i < arrival_result.count; ++i) {
    ArrivalInfo train_data = arrival_result.data[i];
    int minutes = train_data.arrival_time / 60;
    int seconds = train_data.arrival_time % 60;
    size_t station_name_len = strlen(train_data.next_station_name);
    size_t pad_len = LONGEST_STATION_NAME_LENGTH > station_name_len
                         ? LONGEST_STATION_NAME_LENGTH - station_name_len
                         : 0;

    char padding[LONGEST_STATION_NAME_LENGTH + 1] = {0};
    memset(padding, ' ', pad_len);
    padding[pad_len] = '\0';
    print_all("Line", static_cast<int>(train_data.line_number), "to",
              train_data.next_station_name, padding, "arriving in", minutes,
              ":", seconds);
  }
}

void helloWorld() {
  const char HelloWorld[] = "Starting Live Station Monitor";
  display.setFont(&FreeSans12pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(HelloWorld);
  } while (display.nextPage());
}

void setup() {
  init_glyphs();
  Serial.begin(SERIAL_BAUD);

  // Set timezone to Korea (UTC+9)
  setenv("TZ", "KST-9", 1);
  tzset();

  // // Set up wifi
  WiFiManager wifi_manager;

  if (!wifi_manager.autoConnect("LiveStationMonitorWifiSetup")) {
    print_all("Failed to connect and hit timeout");
    delay(2000);
    ESP.restart();
  }

  print_all("\nWifi connected.", "IP address:", WiFi.localIP());

  display.init(115200, true, 2, false);
  display.setRotation(1);
  helloWorld();
}

void draw_vertical_line(
    GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420::HEIGHT>& display, size_t x,
    size_t y_top = 0, size_t y_bottom_offset = 0) {
  display.drawFastVLine(x, y_top, display.height() - y_bottom_offset,
                        GxEPD_BLACK);
}

void draw_horizontal_line(
    GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420::HEIGHT>& display, size_t y) {
  display.drawFastHLine(0, y, display.width(), GxEPD_BLACK);
}

void draw_station_arrivals(
    GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420::HEIGHT>& display,
    const ArrivalResult& arrival_result, const char* station_name, size_t x,
    size_t y) {
  size_t true_count = 0;
  size_t line_height = 32;
  drawHangul(display, x, y, String(station_name));
  display.setFont(&FreeSans24pt7b);
  y += 108;
  x += 4;
  for (auto i = 0; i < arrival_result.count; ++i) {
    ArrivalInfo train = arrival_result.data[i];
    if (strcmp(train.next_station_name, station_name) != 0) {
      continue;
    }
    int minutes = train.arrival_time / 60;
    int seconds = train.arrival_time % 60;

    int yTop = y + true_count * line_height;  // top coordinate for this row

    display.setCursor(x, yTop);
    display.printf("%02d:%02d", minutes, seconds);
    if (true_count == 0) {
      display.setFont(&FreeSans12pt7b);
      x += 3;
    } else if (true_count == 1) {
      x += 30;
    } else {
      return;
    }
    ++true_count;
  }
}

void display_arrivals(
    GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420::HEIGHT>& display,
    const ArrivalResult& arrival_result) {
  display.setPartialWindow(0, 0, display.width(), display.height());
  display.firstPage();
  size_t header_height = 68;
  const size_t header_start_y = 50;
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeSans24pt7b);
    draw_horizontal_line(display, header_height - 6);
    draw_horizontal_line(display, 3);
    draw_vertical_line(display, display.width() / 2, 5);
    display.setCursor(8, header_start_y);
    display.printf("Line 2");
    // drawHangul(display, 8, header_start_y - 48, "2호선");
    display.setCursor(display.width() / 2 + 10, header_start_y);
    display.printf("Line 3");
    // drawHangul(display, display.width() / 2 + 10, header_start_y - 48,
    // "3호선");

    header_height += 6;
    draw_station_arrivals(display, arrival_result, "강남", 10, header_height);
    draw_station_arrivals(display, arrival_result, "고터",
                          display.width() / 2 + 10, header_height);

    size_t middle_line_y = (display.height() + header_height) / 2 - 7;
    draw_horizontal_line(display, middle_line_y);

    draw_station_arrivals(display, arrival_result, "서초", 10,
                          middle_line_y + 15);
    draw_station_arrivals(display, arrival_result, "남부",
                          display.width() / 2 + 10, middle_line_y + 15);

  } while (display.nextPage());
}

bool is_valid_time(size_t start_hour, size_t end_hour) {
  time_t now = time(nullptr);

  struct tm tm_now;
  localtime_r(&now, &tm_now);

  int hour = tm_now.tm_hour;

  return hour >= start_hour && hour < end_hour;
}

void pause_until_valid(
    GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420::HEIGHT>& display,
    size_t start_hour, size_t end_hour) {
  if (is_valid_time(start_hour, end_hour)) {
    return;
  }

  display.fillScreen(GxEPD_WHITE);
  const std::vector<std::string> pause_strings = {
      "Station monitor inactive", "Service will resume at",
      std::to_string(start_hour) + ":00 AM each day."};
  do {
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    for (size_t i = 0; i < pause_strings.size(); ++i) {
      const char* line = pause_strings[i].c_str();
      display.setFont(&FreeSans12pt7b);
      display.getTextBounds(line, 0, 0, &tbx, &tby, &tbw, &tbh);
      // center the bounding box by transposition of the origin:
      uint16_t x = ((display.width() - tbw) / 2) - tbx;
      uint16_t y = ((display.height() - tbh) / 2) - tby -
                   (pause_strings.size() - 1 - i) * 30;
      display.setCursor(x, y);
      display.print(line);
    }
  } while (display.nextPage());

  while (!is_valid_time(start_hour, end_hour)) {
    delay(5000);  // Check every minute
  }
}

void loop() {
  pause_until_valid(display, 6, 21);

  const double_t start_time = millis();
  const double_t loop_delay_millis = LOOP_DELAY_SECONDS * 1000;

  // Fetch and sort arrivals from Seoul Public Data API.
  ArrivalResult arrival_result = fetch_and_sort_arrivals_for_station();
  format_and_print_arrivals(arrival_result);

  // Write to E-ink display.
  display_arrivals(display, arrival_result);

  delay(loop_delay_millis);
}