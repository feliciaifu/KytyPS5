#include "coverLoader.h"

#include "gameContent.h"

#include <QImage>
#include <QSize>

namespace {

constexpr int kMaxCoverWidth = 1280;

QImage LoadCoverImage(const QString& basedir) {
	if (basedir.isEmpty()) {
		return {};
	}

	auto data = GameContent::ReadFile(basedir, QStringLiteral("sce_sys/pic0.png"),
	                                  GameContent::MaxImageSize);
	if (data.isEmpty()) {
		data = GameContent::ReadFile(basedir, QStringLiteral("sce_sys/icon0.png"),
		                             GameContent::MaxImageSize);
	}
	if (data.isEmpty()) {
		return {};
	}

	QImage image;
	if (!image.loadFromData(data)) {
		return {};
	}

	if (image.width() > kMaxCoverWidth) {
		image = image.scaledToWidth(kMaxCoverWidth, Qt::SmoothTransformation);
	}
	return image;
}

} // namespace

QPixmap CoverLoader::Get(const QString& basedir) {
	if (basedir.isEmpty() || m_missing.contains(basedir)) {
		return {};
	}

	if (const auto it = m_cache.constFind(basedir); it != m_cache.constEnd()) {
		return it.value();
	}

	const auto image = LoadCoverImage(basedir);
	if (image.isNull()) {
		m_missing.insert(basedir);
		return {};
	}

	QPixmap pixmap = QPixmap::fromImage(image);
	m_cache.insert(basedir, pixmap);
	return pixmap;
}

void CoverLoader::Clear() {
	m_cache.clear();
	m_missing.clear();
}
