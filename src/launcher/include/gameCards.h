#ifndef GAME_CARDS_H
#define GAME_CARDS_H

#include "configuration.h"

#include <QLabel>
#include <QPixmap>
#include <QWidget>

class ConfigurationItem;
class CoverLoader;
class QPushButton;
class QToolButton;

class ElidedLabel: public QLabel {
	Q_OBJECT

public:
	explicit ElidedLabel(QWidget* parent = nullptr);

	void SetFullText(const QString& text);
	[[nodiscard]] const QString& FullText() const { return m_full_text; }

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void resizeEvent(QResizeEvent* event) override;

private:
	void UpdateElided();

	QString m_full_text;
};

class StatusPill: public QWidget {
	Q_OBJECT

public:
	explicit StatusPill(QWidget* parent = nullptr);

	void SetStatus(Configuration::GameStatus status);
	[[nodiscard]] Configuration::GameStatus GetStatus() const { return m_status; }

	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	Configuration::GameStatus m_status = Configuration::GameStatus::Unknown;
};

class CoverView: public QWidget {
	Q_OBJECT

public:
	explicit CoverView(QWidget* parent = nullptr);

	void SetImage(const QPixmap& image);
	void SetSeed(const QString& seed);
	void SetCornerRadius(qreal radius);

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	QPixmap m_image;
	QString m_seed;
	qreal   m_radius = 10.0;
};

class ToggleSwitch: public QWidget {
	Q_OBJECT

public:
	explicit ToggleSwitch(QWidget* parent = nullptr);

	void SetChecked(bool checked);
	[[nodiscard]] bool IsChecked() const { return m_checked; }

signals:
	void Toggled(bool checked);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void enterEvent(QEnterEvent* event) override;
	void leaveEvent(QEvent* event) override;

private:
	bool m_checked = false;
	bool m_hover   = false;
};

class GameCard: public QWidget {
	Q_OBJECT

public:
	GameCard(ConfigurationItem* item, CoverLoader* covers, QWidget* parent = nullptr);

	[[nodiscard]] ConfigurationItem* GetItem() const { return m_item; }

	void SetSelected(bool selected);
	void SetFavorite(bool favorite);
	void Refresh();
	void LoadCover();

signals:
	void Clicked(GameCard* card);
	void DoubleClicked(GameCard* card);
	void ContextMenuRequested(GameCard* card, const QPoint& global_pos);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void enterEvent(QEnterEvent* event) override;
	void leaveEvent(QEvent* event) override;

private:
	void UpdateTooltip();

	ConfigurationItem* m_item = nullptr;
	CoverLoader*       m_covers = nullptr;

	QString m_title;
	QString m_meta;
	QPixmap m_cover;

	bool m_selected = false;
	bool m_favorite = false;
	bool m_hover    = false;
	bool m_running  = false;
};

class HeroBanner: public QWidget {
	Q_OBJECT

public:
	explicit HeroBanner(QWidget* parent = nullptr);

	void SetItem(ConfigurationItem* item, const QPixmap& cover, const QString& last_played,
	             bool run_enabled);
	void SetRunEnabled(bool enabled);
	void Refresh();

signals:
	void PlayRequested();
	void DetailsRequested();

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	void UpdateTexts();

	ConfigurationItem* m_item = nullptr;
	QPixmap            m_cover;
	QString            m_last_played;

	ElidedLabel* m_title = nullptr;
	QLabel*      m_eyebrow = nullptr;
	QLabel*      m_meta = nullptr;
	QPushButton* m_play = nullptr;
	QToolButton* m_details = nullptr;
	bool         m_run_enabled = true;
};

#endif // GAME_CARDS_H
