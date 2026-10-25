#ifndef LAUNCHER_UI_UTILS_H
#define LAUNCHER_UI_UTILS_H

#include "configuration.h"

#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QIcon>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QStringList>

class QPainter;
class QRectF;

namespace LauncherUi {

[[nodiscard]] QString StatusText(Configuration::GameStatus status);
[[nodiscard]] QColor  StatusColor(Configuration::GameStatus status);
[[nodiscard]] QColor  StatusSoftColor(Configuration::GameStatus status);

[[nodiscard]] QFont UiFont(int pixel_size, QFont::Weight weight = QFont::Normal);
[[nodiscard]] QFont MonoFont(int pixel_size, QFont::Weight weight = QFont::Normal);

void PaintStatusPill(QPainter* painter, const QRectF& rect, Configuration::GameStatus status,
                     int font_pixel_size = 10);
[[nodiscard]] int StatusPillWidth(Configuration::GameStatus status, int font_pixel_size = 10);

[[nodiscard]] QString FormatSize(qint64 bytes);
[[nodiscard]] QString RelativeTime(const QDateTime& time);
[[nodiscard]] QString ElideText(const QFont& font, const QString& text, int width);

[[nodiscard]] QIcon   TintedIcon(const QString& resource, const QColor& color,
                                 const QSize& size = QSize(18, 18));
[[nodiscard]] QPixmap CoverPlaceholder(const QSize& size, const QString& seed);

[[nodiscard]] QString AudioBackendName();
[[nodiscard]] QStringList QueryAudioPlaybackDevices();

void ApplyPrimaryShadow(QWidget* widget);

struct VulkanDevice {
	QString name;
	QString api;
};

struct VulkanInfo {
	bool    valid = false;
	QString device;
	QString version;
};

[[nodiscard]] VulkanInfo QueryVulkanInfo();
[[nodiscard]] QList<VulkanDevice> QueryVulkanDevices();

} // namespace LauncherUi

#endif // LAUNCHER_UI_UTILS_H
