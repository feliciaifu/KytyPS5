#include "common/debugHttpServer.h"

#include "common/common.h"

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS

#include "common/logging/log.h"
#include "graphics/presentation/window.h"
#include "graphics/presentation/window/hostInput.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <iterator>
#include <fstream>
#include <fmt/format.h>
#include <map>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace Common {
namespace {

constexpr uint16_t DefaultPort = 8765;

std::string JsonEscape(const std::string& input) {
	std::string result;
	result.reserve(input.size() + 8);
	for (const auto ch: input) {
		switch (ch) {
			case '\\': result += "\\\\"; break;
			case '"': result += "\\\""; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			default:
				if (static_cast<unsigned char>(ch) < 0x20) {
					const auto text = fmt::format("\\u{:04x}", static_cast<unsigned int>(ch));
					result += text;
				} else {
					result += ch;
				}
				break;
		}
	}
	return result;
}

std::string UrlDecode(const std::string& input) {
	std::string output;
	output.reserve(input.size());
	for (size_t i = 0; i < input.size(); ++i) {
		if (input[i] == '%' && i + 2 < input.size()) {
			const auto hex = input.substr(i + 1, 2);
			char*      end = nullptr;
			const auto value = std::strtol(hex.c_str(), &end, 16);
			if (end != nullptr && *end == '\0') {
				output += static_cast<char>(value);
				i += 2;
				continue;
			}
		}
		output += input[i] == '+' ? ' ' : input[i];
	}
	return output;
}

using Query = std::map<std::string, std::string>;

Query ParseQuery(const std::string& text) {
	Query result;
	size_t start = 0;
	while (start <= text.size()) {
		const auto end = text.find('&', start);
		const auto part = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
		const auto equals = part.find('=');
		if (equals != std::string::npos) {
			result[UrlDecode(part.substr(0, equals))] = UrlDecode(part.substr(equals + 1));
		} else if (!part.empty()) {
			result[UrlDecode(part)] = "";
		}
		if (end == std::string::npos) {
			break;
		}
		start = end + 1;
	}
	return result;
}

std::string QueryValue(const Query& query, const char* name, const char* fallback = "") {
	const auto found = query.find(name);
	return found != query.end() ? found->second : fallback;
}

bool QueryBool(const Query& query, const char* name, bool fallback) {
	const auto value = QueryValue(query, name, fallback ? "1" : "0");
	return value == "1" || value == "true" || value == "on" || value == "down";
}

HWND FindGameWindow() {
	struct State {
		DWORD process_id;
		HWND  best;
		LONG  best_area;
		bool  matched = false;
	} state {GetCurrentProcessId(), nullptr, 0, false};

	EnumWindows(
	    [](HWND window, LPARAM param) -> BOOL {
		    auto* state = reinterpret_cast<State*>(param);
		    DWORD process_id = 0;
		    GetWindowThreadProcessId(window, &process_id);
		    if (process_id != state->process_id || IsWindowVisible(window) == 0 ||
		        GetWindowTextLengthW(window) == 0) {
			    return TRUE;
		    }
		    wchar_t class_name[128] = {};
		    GetClassNameW(window, class_name, static_cast<int>(std::size(class_name)));
		    if (wcsstr(class_name, L"Console") != nullptr) {
			    return TRUE;
		    }
		    wchar_t title[512] = {};
		    GetWindowTextW(window, title, static_cast<int>(std::size(title)));
		    const bool preferred = wcsstr(title, L"fps:") != nullptr ||
		                           wcsstr(title, L"Source build") != nullptr ||
		                           wcsstr(title, L"Demon") != nullptr;
		    RECT client {};
		    if (GetClientRect(window, &client) == 0) {
			    return TRUE;
		    }
		    const auto area = (client.right - client.left) * (client.bottom - client.top);
		    if ((preferred && !state->matched) || (preferred == state->matched && area > state->best_area)) {
			    state->best = window;
			    state->best_area = area;
			    state->matched = preferred;
		    }
		    return TRUE;
	    },
	    reinterpret_cast<LPARAM>(&state));
	return state.best;
}

bool FocusGameWindow() {
	const auto window = FindGameWindow();
	if (window == nullptr) {
		return false;
	}
	const auto foreground = GetForegroundWindow();
	const auto foreground_thread =
	    foreground != nullptr ? GetWindowThreadProcessId(foreground, nullptr) : 0;
	const auto current_thread = GetCurrentThreadId();
	const bool attached = foreground_thread != 0 && foreground_thread != current_thread &&
	                      AttachThreadInput(current_thread, foreground_thread, TRUE) != 0;
	ShowWindow(window, SW_RESTORE);
	SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	SetForegroundWindow(window);
	SetActiveWindow(window);
	SetFocus(window);
	if (attached) {
		AttachThreadInput(current_thread, foreground_thread, FALSE);
	}
	return GetForegroundWindow() == window;
}

std::string WindowTitle() {
	const auto window = FindGameWindow();
	if (window == nullptr) {
		return {};
	}
	thread_local wchar_t buffer[1024] = {};
	const auto length = GetWindowTextW(window, buffer, std::size(buffer));
	if (length <= 0) {
		return {};
	}
	const auto size = WideCharToMultiByte(CP_UTF8, 0, buffer, length, nullptr, 0, nullptr, nullptr);
	std::string result(static_cast<size_t>(size), '\0');
	WideCharToMultiByte(CP_UTF8, 0, buffer, length, result.data(), size, nullptr, nullptr);
	return result;
}

bool CaptureWindow(const std::filesystem::path& path, uint32_t& width, uint32_t& height) {
	FocusGameWindow();
	const auto window = FindGameWindow();
	if (window == nullptr) {
		return false;
	}
	RECT client {};
	if (GetClientRect(window, &client) == 0) {
		return false;
	}
	width  = static_cast<uint32_t>(std::max<LONG>(client.right - client.left, 0));
	height = static_cast<uint32_t>(std::max<LONG>(client.bottom - client.top, 0));
	if (width == 0 || height == 0) {
		return false;
	}
	POINT origin {client.left, client.top};
	if (ClientToScreen(window, &origin) == 0) {
		return false;
	}
	const auto screen = GetDC(nullptr);
	if (screen == nullptr) {
		return false;
	}
	const auto memory = CreateCompatibleDC(screen);
	const auto bitmap = CreateCompatibleBitmap(screen, static_cast<int>(width), static_cast<int>(height));
	if (memory == nullptr || bitmap == nullptr) {
		if (memory != nullptr) DeleteDC(memory);
		if (bitmap != nullptr) DeleteObject(bitmap);
		ReleaseDC(nullptr, screen);
		return false;
	}
	const auto old = SelectObject(memory, bitmap);
	BitBlt(memory, 0, 0, static_cast<int>(width), static_cast<int>(height), screen, origin.x,
	       origin.y, SRCCOPY);
	SelectObject(memory, old);

	std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
	BITMAPINFO           info {};
	info.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth       = static_cast<LONG>(width);
	info.bmiHeader.biHeight      = static_cast<LONG>(height);
	info.bmiHeader.biPlanes      = 1;
	info.bmiHeader.biBitCount    = 32;
	info.bmiHeader.biCompression = BI_RGB;
	const auto lines = GetDIBits(memory, bitmap, 0, height, pixels.data(), &info, DIB_RGB_COLORS);

	DeleteDC(memory);
	DeleteObject(bitmap);
	ReleaseDC(nullptr, screen);
	if (lines == 0) {
		return false;
	}

	std::error_code error;
	std::filesystem::create_directories(path.parent_path(), error);
	std::ofstream file(path, std::ios::binary);
	if (!file) {
		return false;
	}
	BITMAPFILEHEADER header {};
	header.bfType    = 0x4d42;
	header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	header.bfSize    = header.bfOffBits + static_cast<DWORD>(pixels.size());
	file.write(reinterpret_cast<const char*>(&header), sizeof(header));
	file.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
	file.write(reinterpret_cast<const char*>(pixels.data()),
	           static_cast<std::streamsize>(pixels.size()));
	return file.good();
}

void SendAll(SOCKET socket, const std::string& data) {
	size_t offset = 0;
	while (offset < data.size()) {
		const auto sent = send(socket, data.data() + offset,
		                       static_cast<int>(data.size() - offset), 0);
		if (sent <= 0) {
			return;
		}
		offset += static_cast<size_t>(sent);
	}
}

void Respond(SOCKET socket, int status, const char* reason, const char* type,
             const std::string& body) {
	const auto header = fmt::format("HTTP/1.1 {} {}\r\nContent-Type: {}\r\nContent-Length: {}\r\n"
	                                "Connection: close\r\nAccess-Control-Allow-Origin: *\r\n\r\n",
	                                status, reason, type, body.size());
	SendAll(socket, header + body);
}

void RespondJson(SOCKET socket, int status, const char* reason, const std::string& body) {
	Respond(socket, status, reason, "application/json", body);
}

int KeyCodeForName(const std::string& name) {
	if (name.empty()) {
		return -1;
	}
	char*      end = nullptr;
	const auto numeric = std::strtol(name.c_str(), &end, 0);
	if (end != nullptr && *end == '\0') {
		return static_cast<int>(numeric);
	}
	if (name.size() == 1) {
		const auto ch = name[0];
		if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
			return ch;
		}
	}
	if (name == "space") return 0x20;
	if (name == "enter" || name == "return") return 0x0d;
	if (name == "escape" || name == "esc") return 0x1b;
	if (name == "tab") return 0x09;
	if (name == "backspace") return 0x08;
	if (name == "up") return 0x40000052;
	if (name == "down") return 0x40000051;
	if (name == "left") return 0x40000050;
	if (name == "right") return 0x4000004f;
	if (name == "lshift" || name == "shift") return 0x400000e1;
	if (name == "lctrl" || name == "ctrl") return 0x400000e0;
	if (name == "lalt" || name == "alt") return 0x400000e2;
	for (int i = 1; i <= 12; ++i) {
		if (name == fmt::format("f{}", i)) {
			return 0x40000039 + i;
		}
	}
	return -1;
}

bool SendKey(const std::string& name, bool down) {
	const auto key = KeyCodeForName(name);
	if (key < 0) {
		return false;
	}
	Libs::Graphics::HostInputQueueKey(key, down);
	return true;
}

int PadButton(const std::string& name) {
	if (name == "cross" || name == "south" || name == "a") return 0;
	if (name == "circle" || name == "east" || name == "b") return 1;
	if (name == "square" || name == "west" || name == "x") return 2;
	if (name == "triangle" || name == "north" || name == "y") return 3;
	if (name == "l1") return 9;
	if (name == "r1") return 10;
	if (name == "l3") return 7;
	if (name == "r3") return 8;
	if (name == "options" || name == "start") return 6;
	if (name == "up") return 11;
	if (name == "down") return 12;
	if (name == "left") return 13;
	if (name == "right") return 14;
	return -1;
}

void HandleRequest(SOCKET socket, const std::string& request) {
	const auto line_end = request.find("\r\n");
	if (line_end == std::string::npos) {
		RespondJson(socket, 400, "Bad Request", R"({"ok":false,"error":"bad request"})");
		return;
	}
	const auto line = request.substr(0, line_end);
	const auto first_space = line.find(' ');
	const auto last_space  = line.rfind(' ');
	if (first_space == std::string::npos || last_space == std::string::npos ||
	    first_space == last_space) {
		RespondJson(socket, 400, "Bad Request", R"({"ok":false,"error":"bad request"})");
		return;
	}
	const auto method = line.substr(0, first_space);
	const auto target = line.substr(first_space + 1, last_space - first_space - 1);
	if (method != "GET") {
		RespondJson(socket, 405, "Method Not Allowed", R"({"ok":false,"error":"GET only"})");
		return;
	}
	const auto question = target.find('?');
	const auto path     = question == std::string::npos ? target : target.substr(0, question);
	const auto query    = ParseQuery(question == std::string::npos ? "" : target.substr(question + 1));

	if (path == "/status") {
		const auto title = WindowTitle();
		RespondJson(socket, 200, "OK",
		            fmt::format(R"({{"ok":true,"pid":{},"window":{},"title":"{}"}})",
		                        GetCurrentProcessId(), title.empty() ? "false" : "true",
		                        JsonEscape(title)));
		return;
	}
	if (path == "/focus") {
		const auto ok = FocusGameWindow();
		RespondJson(socket, ok ? 200 : 503, ok ? "OK" : "Service Unavailable",
		            fmt::format(R"({{"ok":{}}})", ok ? "true" : "false"));
		return;
	}
	if (path == "/key") {
		const auto name = QueryValue(query, "code", QueryValue(query, "key").c_str());
		const auto down = QueryBool(query, "down", true);
		if (!SendKey(name, down)) {
			RespondJson(socket, 400, "Bad Request",
			            R"({"ok":false,"error":"unknown key or window not ready"})");
		} else {
			RespondJson(socket, 200, "OK", R"({"ok":true})");
		}
		return;
	}
	if (path == "/pad") {
		const auto name = QueryValue(query, "button");
		const auto down = QueryBool(query, "down", true);
		const auto id   = std::max(std::strtol(QueryValue(query, "id", "0").c_str(), nullptr, 10),
		                          0L);
		const auto button = PadButton(name);
		if (button < 0) {
			RespondJson(socket, 400, "Bad Request", R"({"ok":false,"error":"unknown button"})");
			return;
		}
		Libs::Graphics::HostInputQueuePad(static_cast<int>(id), button, down);
		RespondJson(socket, 200, "OK", R"({"ok":true})");
		return;
	}
	if (path == "/screenshot") {
		auto path_text = QueryValue(query, "path");
		std::filesystem::path output =
		    path_text.empty() ? std::filesystem::temp_directory_path() / "kyty_http.bmp"
		                      : std::filesystem::u8path(path_text);
		uint32_t width  = 0;
		uint32_t height = 0;
		if (!CaptureWindow(output, width, height)) {
			RespondJson(socket, 503, "Service Unavailable",
			            R"({"ok":false,"error":"window capture failed"})");
			return;
		}
		RespondJson(socket, 200, "OK",
		            fmt::format(R"({{"ok":true,"width":{},"height":{},"path":"{}"}})", width,
		                        height, JsonEscape(output.string())));
		return;
	}
	RespondJson(socket, 404, "Not Found", R"({"ok":false,"error":"not found"})");
}

void ServerLoop(uint16_t port) {
	WSADATA data {};
	if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
		return;
	}
	const auto listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener == INVALID_SOCKET) {
		WSACleanup();
		return;
	}
	const BOOL reuse = TRUE;
	setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
	           sizeof(reuse));
	sockaddr_in address {};
	address.sin_family      = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address.sin_port        = htons(port);
	if (bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) ==
	        SOCKET_ERROR ||
	    listen(listener, 8) == SOCKET_ERROR) {
		closesocket(listener);
		WSACleanup();
		return;
	}
	Log::WriteToConsoleAndLog(fmt::format("Debug HTTP: http://127.0.0.1:{}/\n", port));
	for (;;) {
		const auto client = accept(listener, nullptr, nullptr);
		if (client == INVALID_SOCKET) {
			continue;
		}
		std::array<char, 16384> buffer {};
		size_t                  size = 0;
		while (size < buffer.size()) {
			const auto received = recv(client, buffer.data() + size,
			                           static_cast<int>(buffer.size() - size), 0);
			if (received <= 0) {
				break;
			}
			size += static_cast<size_t>(received);
			if (std::string_view(buffer.data(), size).find("\r\n\r\n") != std::string_view::npos) {
				break;
			}
		}
		HandleRequest(client, std::string(buffer.data(), size));
		closesocket(client);
	}
}

} // namespace

void StartDebugHttpServer() {
	const auto* setting = std::getenv("KYTY_DEBUG_HTTP");
	if (setting == nullptr) {
		setting = std::getenv("KYTY_HTTP_PORT");
	}
	if (setting == nullptr || *setting == '\0' || std::strcmp(setting, "0") == 0 ||
	    std::strcmp(setting, "false") == 0 || std::strcmp(setting, "off") == 0) {
		return;
	}
	uint16_t port = DefaultPort;
	if (std::strcmp(setting, "1") != 0 && std::strcmp(setting, "true") != 0 &&
	    std::strcmp(setting, "on") != 0) {
		const auto value = std::strtol(setting, nullptr, 10);
		if (value <= 0 || value > 65535) {
			return;
		}
		port = static_cast<uint16_t>(value);
	}
	std::thread(ServerLoop, port).detach();
}

} // namespace Common

#endif
