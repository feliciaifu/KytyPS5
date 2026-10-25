#include "launcherTheme.h"

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QFile>
#include <QObject>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>
#include <QTimer>

namespace {

class PaletteChangeFilter final: public QObject {
public:
	explicit PaletteChangeFilter(QApplication& application)
	    : QObject(&application), m_application(application) {}

protected:
	bool eventFilter(QObject* watched, QEvent* event) override {
		if (event->type() == QEvent::ApplicationPaletteChange && !m_refresh_pending) {
			m_refresh_pending = true;
			QTimer::singleShot(0, &m_application, [this] {
				m_application.setStyleSheet(m_application.styleSheet());
				m_refresh_pending = false;
			});
		}

		return QObject::eventFilter(watched, event);
	}

private:
	QApplication& m_application;
	bool          m_refresh_pending = false;
};

} // namespace

namespace LauncherTheme {

void Initialize(QApplication& application) {
	application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
	application.styleHints()->setColorScheme(Qt::ColorScheme::Dark);

	QPalette palette;
	palette.setColor(QPalette::Window, QColor(QStringLiteral("#07090D")));
	palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#F2F5FA")));
	palette.setColor(QPalette::Base, QColor(QStringLiteral("#0E121A")));
	palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#10141C")));
	palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#10141C")));
	palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#DDE3EF")));
	palette.setColor(QPalette::Text, QColor(QStringLiteral("#DDE3EF")));
	palette.setColor(QPalette::Button, QColor(QStringLiteral("#171E2B")));
	palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#C9D2E2")));
	palette.setColor(QPalette::BrightText, QColor(QStringLiteral("#FFFFFF")));
	palette.setColor(QPalette::Link, QColor(QStringLiteral("#7FB3FF")));
	palette.setColor(QPalette::LinkVisited, QColor(QStringLiteral("#8B5CF6")));
	palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#16233A")));
	palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#F2F5FA")));
	palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#5F6A80")));
	palette.setColor(QPalette::Mid, QColor(QStringLiteral("#232B3A")));
	palette.setColor(QPalette::Dark, QColor(QStringLiteral("#0B0E14")));
	palette.setColor(QPalette::Light, QColor(QStringLiteral("#2A3344")));

	for (auto role: {QPalette::WindowText, QPalette::Text, QPalette::ButtonText,
	                 QPalette::ToolTipText}) {
		palette.setColor(QPalette::Disabled, role, QColor(QStringLiteral("#4E5769")));
	}
	application.setPalette(palette);

	QFile style_sheet_file(QStringLiteral(":/styles/kytyps5-console.qss"));
	if (style_sheet_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		application.setStyleSheet(QString::fromUtf8(style_sheet_file.readAll()));
		application.installEventFilter(new PaletteChangeFilter(application));
	}
}
} // namespace LauncherTheme
