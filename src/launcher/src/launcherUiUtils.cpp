#include "launcherUiUtils.h"

#include <SDL3/SDL.h>

#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QWidget>
#include <QList>
#include <QLocale>
#include <QScreen>
#include <QVersionNumber>
#include <QGuiApplication>
#include <QIcon>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QVulkanFunctions>
#include <QVulkanInstance>

#include <vulkan/vulkan.h>

namespace {

const QColor kBackground(0x07, 0x09, 0x0d);
const QColor kPanel(0x0e, 0x12, 0x1a);
const QColor kAccent(0x3d, 0x8b, 0xff);
const QColor kAccentSoft(0x8b, 0x5c, 0xf6);
const QColor kText(0xf2, 0xf5, 0xfa);
const QColor kTextMuted(0x9b, 0xa4, 0xb8);
const QColor kTextWeak(0x5f, 0x6a, 0x80);

} // namespace

namespace LauncherUi {

QString StatusText(Configuration::GameStatus status) {
	switch (status) {
		case Configuration::GameStatus::InGame: return QStringLiteral("可玩");
		case Configuration::GameStatus::MainMenu: return QStringLiteral("主菜单");
		case Configuration::GameStatus::Logo: return QStringLiteral("卡 Logo");
		case Configuration::GameStatus::DoesntBoot: return QStringLiteral("无法启动");
		case Configuration::GameStatus::Unknown: return QStringLiteral("未测试");
	}

	return QStringLiteral("未测试");
}

QColor StatusColor(Configuration::GameStatus status) {
	switch (status) {
		case Configuration::GameStatus::InGame: return QColor(QStringLiteral("#37D399"));
		case Configuration::GameStatus::MainMenu: return QColor(QStringLiteral("#3D8BFF"));
		case Configuration::GameStatus::Logo: return QColor(QStringLiteral("#F2B33D"));
		case Configuration::GameStatus::DoesntBoot: return QColor(QStringLiteral("#EF5A6F"));
		case Configuration::GameStatus::Unknown: return QColor(QStringLiteral("#6B7488"));
	}

	return QColor(QStringLiteral("#6B7488"));
}

QColor StatusSoftColor(Configuration::GameStatus status) {
	switch (status) {
		case Configuration::GameStatus::InGame: return QColor(QStringLiteral("#7BE3BE"));
		case Configuration::GameStatus::MainMenu: return QColor(QStringLiteral("#9CC2FF"));
		case Configuration::GameStatus::Logo: return QColor(QStringLiteral("#FFD98A"));
		case Configuration::GameStatus::DoesntBoot: return QColor(QStringLiteral("#FF9AA8"));
		case Configuration::GameStatus::Unknown: return QColor(QStringLiteral("#A7B0C2"));
	}

	return QColor(QStringLiteral("#A7B0C2"));
}

QFont UiFont(int pixel_size, QFont::Weight weight) {
	QFont font;
	font.setFamilies({QStringLiteral("Noto Sans SC"), QStringLiteral("Microsoft YaHei UI"),
	                  QStringLiteral("Segoe UI")});
	font.setPixelSize(pixel_size);
	font.setWeight(weight);
	return font;
}

QFont MonoFont(int pixel_size, QFont::Weight weight) {
	QFont font;
	font.setFamilies({QStringLiteral("Inter"), QStringLiteral("Segoe UI"),
	                  QStringLiteral("Microsoft YaHei UI")});
	font.setPixelSize(pixel_size);
	font.setWeight(weight);
	return font;
}

void PaintStatusPill(QPainter* painter, const QRectF& rect, Configuration::GameStatus status,
                     int font_pixel_size) {
	const auto pill_rect = rect.adjusted(0.5, 0.5, -0.5, -0.5);
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setPen(QPen(QColor(255, 255, 255, 36), 1));
	painter->setBrush(QColor(2, 3, 5, 158));
	painter->drawRoundedRect(pill_rect, 6, 6);
	painter->setPen(Qt::NoPen);

	const auto dot_color = StatusColor(status);
	const qreal dot_size  = 5.0;
	const QRectF dot_rect(pill_rect.left() + 9, pill_rect.center().y() - dot_size / 2.0, dot_size,
	                      dot_size);
	painter->setBrush(dot_color);
	painter->drawEllipse(dot_rect);

	auto font = UiFont(font_pixel_size, QFont::Medium);
	painter->setFont(font);
	painter->setPen(StatusSoftColor(status));
	const QRectF text_rect(pill_rect.left() + 20, pill_rect.top(), pill_rect.width() - 28,
	                       pill_rect.height());
	painter->drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, StatusText(status));
	painter->restore();
}

int StatusPillWidth(Configuration::GameStatus status, int font_pixel_size) {
	const QFontMetrics metrics(UiFont(font_pixel_size, QFont::Medium));
	return 20 + metrics.horizontalAdvance(StatusText(status)) + 9;
}

QString FormatSize(qint64 bytes) {
	if (bytes < 0) {
		return QStringLiteral("\u2014");
	}
	return QLocale().formattedDataSize(bytes, 2, QLocale::DataSizeTraditionalFormat);
}

QString RelativeTime(const QDateTime& time) {
	if (!time.isValid()) {
		return {};
	}

	const auto now = QDateTime::currentDateTime();
	if (time > now) {
		return time.toString(QStringLiteral("yyyy-MM-dd"));
	}

	const auto seconds = time.secsTo(now);
	if (seconds < 60) {
		return QStringLiteral("刚刚");
	}
	if (seconds < 3600) {
		return QStringLiteral("%1 分钟前").arg(seconds / 60);
	}
	if (seconds < 86400) {
		return QStringLiteral("%1 小时前").arg(seconds / 3600);
	}
	if (seconds < 86400 * 7) {
		return QStringLiteral("%1 天前").arg(seconds / 86400);
	}
	return time.toString(QStringLiteral("yyyy-MM-dd"));
}

QString ElideText(const QFont& font, const QString& text, int width) {
	return QFontMetrics(font).elidedText(text, Qt::ElideRight, qMax(0, width));
}

QIcon TintedIcon(const QString& resource, const QColor& color, const QSize& size) {
	const qreal dpr = QGuiApplication::primaryScreen() != nullptr ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
	auto        pixmap = QIcon(resource).pixmap(size, dpr);
	if (pixmap.isNull()) {
		return {};
	}
	QPainter painter(&pixmap);
	painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
	painter.fillRect(pixmap.rect(), color);
	painter.end();
	pixmap.setDevicePixelRatio(dpr);
	return QIcon(pixmap);
}

QPixmap CoverPlaceholder(const QSize& size, const QString& seed) {
	QPixmap pixmap(qMax(1, size.width()), qMax(1, size.height()));
	pixmap.fill(Qt::transparent);

	const auto hue = static_cast<int>(qHash(seed) % 360);
	QLinearGradient gradient(0, 0, pixmap.width(), pixmap.height());
	gradient.setColorAt(0.0, QColor::fromHsv(hue, 90, 46));
	gradient.setColorAt(0.55, QColor::fromHsv((hue + 32) % 360, 100, 30));
	gradient.setColorAt(1.0, QColor(0x0e, 0x12, 0x1a));

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.fillRect(pixmap.rect(), gradient);

	painter.setPen(QPen(QColor(255, 255, 255, 18), 1));
	const int step = qMax(28, pixmap.width() / 12);
	for (int x = -pixmap.height(); x < pixmap.width(); x += step) {
		painter.drawLine(x, 0, x + pixmap.height(), pixmap.height());
	}

	const auto text = seed.isEmpty() ? QStringLiteral("K") : seed.left(1).toUpper();
	auto       font = UiFont(qMax(18, pixmap.height() / 4), QFont::DemiBold);
	painter.setFont(font);
	painter.setPen(QColor(255, 255, 255, 38));
	painter.drawText(pixmap.rect(), Qt::AlignCenter, text);
	return pixmap;
}

void ApplyPrimaryShadow(QWidget* widget) {
	if (widget == nullptr) {
		return;
	}
	auto* effect = new QGraphicsDropShadowEffect(widget);
	effect->setBlurRadius(18);
	effect->setOffset(0, 6);
	effect->setColor(QColor(61, 139, 255, 89));
	widget->setGraphicsEffect(effect);
}

QString AudioBackendName() {
#if defined(_WIN32)
	return QStringLiteral("SDL3 · WASAPI");
#elif defined(__APPLE__)
	return QStringLiteral("SDL3 · CoreAudio");
#else
	return QStringLiteral("SDL3 · PipeWire / ALSA");
#endif
}

QStringList QueryAudioPlaybackDevices() {
	QStringList devices;

	const bool was_initialized = SDL_WasInit(SDL_INIT_AUDIO) != 0;
	if (!was_initialized && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		return devices;
	}

	int                count = 0;
	SDL_AudioDeviceID* list  = SDL_GetAudioPlaybackDevices(&count);
	if (list != nullptr) {
		for (int index = 0; index < count; index++) {
			const char* name = SDL_GetAudioDeviceName(list[index]);
			if (name != nullptr && name[0] != '\0') {
				devices.append(QString::fromUtf8(name));
			}
		}
		SDL_free(list);
	}

	if (!was_initialized) {
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}
	return devices;
}

QList<VulkanDevice> QueryVulkanDevices() {
	QList<VulkanDevice> devices;

	QVulkanInstance instance;
	instance.setApiVersion(QVersionNumber(1, 3, 0));
	if (!instance.create()) {
		return devices;
	}

	auto* functions = instance.functions();
	if (functions == nullptr) {
		instance.destroy();
		return devices;
	}

	uint32_t count = 0;
	if (functions->vkEnumeratePhysicalDevices(instance.vkInstance(), &count, nullptr) !=
	        VK_SUCCESS ||
	    count == 0) {
		instance.destroy();
		return devices;
	}

	QList<VkPhysicalDevice> handles(static_cast<int>(count));
	if (functions->vkEnumeratePhysicalDevices(instance.vkInstance(), &count, handles.data()) !=
	    VK_SUCCESS) {
		instance.destroy();
		return devices;
	}

	for (const auto& handle: handles) {
		VkPhysicalDeviceProperties properties {};
		functions->vkGetPhysicalDeviceProperties(handle, &properties);
		devices.append({QString::fromUtf8(properties.deviceName),
		                QStringLiteral("Vulkan %1.%2")
		                    .arg(VK_VERSION_MAJOR(properties.apiVersion))
		                    .arg(VK_VERSION_MINOR(properties.apiVersion))});
	}
	instance.destroy();
	return devices;
}

VulkanInfo QueryVulkanInfo() {
	const auto devices = QueryVulkanDevices();
	if (devices.isEmpty()) {
		return {};
	}
	VulkanInfo info;
	info.valid   = true;
	info.device  = devices.first().name;
	info.version = devices.first().api;
	return info;
}

} // namespace LauncherUi
