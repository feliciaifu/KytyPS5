#ifndef COVER_LOADER_H
#define COVER_LOADER_H

#include <QHash>
#include <QPixmap>
#include <QSet>
#include <QString>

class CoverLoader {
public:
	[[nodiscard]] QPixmap Get(const QString& basedir);
	void                      Clear();

private:
	QHash<QString, QPixmap> m_cache;
	QSet<QString>           m_missing;
};

#endif // COVER_LOADER_H
