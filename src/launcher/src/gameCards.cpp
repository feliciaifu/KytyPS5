#include "gameCards.h"

#include "configurationItem.h"
#include "coverLoader.h"
#include "gameViews.h"
#include "launcherUiUtils.h"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kCardHeight = 260;
constexpr int kCardPadding = 12;
constexpr int kCardInfoHeight = 53;

void DrawCover(QPainter* painter, const QRectF& rect, const QPixmap& pixmap, const QString& seed,
               qreal radius) {
	QPainterPath clip;
	clip.addRoundedRect(rect, radius, radius);
	QPainterPath square_bottom;
	square_bottom.addRect(
	    QRectF(rect.left(), rect.bottom() - radius, rect.width(), radius));
	clip = clip.united(square_bottom);

	painter->save();
	painter->setClipPath(clip);

	if (pixmap.isNull()) {
		painter->drawPixmap(rect.toRect(),
		                    LauncherUi::CoverPlaceholder(rect.size().toSize(), seed));
	} else {
		const auto target = rect.size().toSize();
		const auto scaled = pixmap.scaled(target, Qt::KeepAspectRatioByExpanding,
		                                  Qt::SmoothTransformation);
		const QRect source((scaled.width() - target.width()) / 2,
		                   (scaled.height() - target.height()) / 2, target.width(),
		                   target.height());
		painter->drawPixmap(rect, scaled, QRectF(source));
	}

	QLinearGradient fade(0, rect.bottom() - 42, 0, rect.bottom());
	fade.setColorAt(0.0, QColor(7, 9, 13, 0));
	fade.setColorAt(1.0, QColor(7, 9, 13, 125));
	painter->fillRect(QRectF(rect.left(), rect.bottom() - 42, rect.width(), 42), fade);
	painter->restore();
}

} // namespace

ElidedLabel::ElidedLabel(QWidget* parent): QLabel(parent) {
	setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	setMinimumWidth(0);
	setWordWrap(false);
}

void ElidedLabel::SetFullText(const QString& text) {
	m_full_text = text;
	UpdateElided();
}

QSize ElidedLabel::sizeHint() const {
	return QSize(QFontMetrics(font()).horizontalAdvance(m_full_text),
	             QLabel::sizeHint().height());
}

QSize ElidedLabel::minimumSizeHint() const {
	return QSize(0, QLabel::minimumSizeHint().height());
}

void ElidedLabel::resizeEvent(QResizeEvent* event) {
	QLabel::resizeEvent(event);
	UpdateElided();
}

void ElidedLabel::UpdateElided() {
	if (width() <= 0) {
		setText(m_full_text);
		return;
	}
	setText(LauncherUi::ElideText(font(), m_full_text, width()));
}

StatusPill::StatusPill(QWidget* parent): QWidget(parent) {
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	setFixedHeight(22);
	SetStatus(m_status);
}

void StatusPill::SetStatus(Configuration::GameStatus status) {
	m_status = status;
	setFixedWidth(LauncherUi::StatusPillWidth(status, 10));
	update();
}

QSize StatusPill::sizeHint() const {
	return QSize(LauncherUi::StatusPillWidth(m_status, 10), 22);
}

void StatusPill::paintEvent(QPaintEvent* /*event*/) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const auto width = qMin(this->width(), LauncherUi::StatusPillWidth(m_status, 10));
	LauncherUi::PaintStatusPill(&painter, QRectF(0.5, 0.5, width - 1.0, height() - 1.0), m_status,
	                            10);
}

CoverView::CoverView(QWidget* parent): QWidget(parent) {
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void CoverView::SetImage(const QPixmap& image) {
	m_image = image;
	update();
}

void CoverView::SetSeed(const QString& seed) {
	m_seed = seed;
	update();
}

void CoverView::SetCornerRadius(qreal radius) {
	m_radius = radius;
	update();
}

void CoverView::paintEvent(QPaintEvent* /*event*/) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	QPainterPath clip;
	clip.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), m_radius, m_radius);
	painter.setClipPath(clip);

	if (m_image.isNull()) {
		painter.drawPixmap(rect(), LauncherUi::CoverPlaceholder(size(), m_seed));
	} else {
		const auto scaled =
		    m_image.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
		const QRect source((scaled.width() - width()) / 2, (scaled.height() - height()) / 2,
		                   width(), height());
		painter.drawPixmap(rect(), scaled, QRectF(source));
	}

	painter.setClipping(false);
	painter.setPen(QPen(QColor(255, 255, 255, 18), 1));
	painter.setBrush(Qt::NoBrush);
	painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), m_radius, m_radius);
}

ToggleSwitch::ToggleSwitch(QWidget* parent): QWidget(parent) {
	setFixedSize(34, 20);
	setCursor(Qt::PointingHandCursor);
	setFocusPolicy(Qt::NoFocus);
}

void ToggleSwitch::SetChecked(bool checked) {
	if (m_checked == checked) {
		return;
	}
	m_checked = checked;
	update();
}

void ToggleSwitch::paintEvent(QPaintEvent* /*event*/) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	const QRectF track(0.5, 0.5, width() - 1.0, height() - 1.0);
	painter.setPen(Qt::NoPen);
	painter.setBrush(m_checked ? QColor(QStringLiteral("#3D8BFF"))
	                           : (m_hover ? QColor(QStringLiteral("#343E50"))
	                                      : QColor(QStringLiteral("#2A3140"))));
	painter.drawRoundedRect(track, 10, 10);

	const qreal knob_x = m_checked ? width() - 18.0 : 2.0;
	painter.setBrush(QColor(QStringLiteral("#FFFFFF")));
	painter.drawEllipse(QRectF(knob_x, 2.0, 16.0, 16.0));
}

void ToggleSwitch::mouseReleaseEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton) {
		m_checked = !m_checked;
		update();
		emit Toggled(m_checked);
	}
	QWidget::mouseReleaseEvent(event);
}

void ToggleSwitch::enterEvent(QEnterEvent* event) {
	m_hover = true;
	update();
	QWidget::enterEvent(event);
}

void ToggleSwitch::leaveEvent(QEvent* event) {
	m_hover = false;
	update();
	QWidget::leaveEvent(event);
}

GameCard::GameCard(ConfigurationItem* item, CoverLoader* covers, QWidget* parent)
    : QWidget(parent), m_item(item), m_covers(covers) {
	setFixedHeight(kCardHeight);
	setMinimumWidth(200);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setCursor(Qt::PointingHandCursor);
	Refresh();
}

void GameCard::SetSelected(bool selected) {
	if (m_selected == selected) {
		return;
	}
	m_selected = selected;
	update();
}

void GameCard::SetFavorite(bool favorite) {
	if (m_favorite == favorite) {
		return;
	}
	m_favorite = favorite;
	update();
}

void GameCard::Refresh() {
	if (m_item == nullptr) {
		return;
	}

	const auto& info = m_item->GetInfo();
	m_title          = info.name;
	m_running        = m_item->IsRunning();

	const auto serial = info.title_id;
	const auto size   = m_item->text(GAME_SIZE_COLUMN);
	m_meta.clear();
	if (!serial.isEmpty()) {
		m_meta = serial;
	}
	if (!size.isEmpty() && size != QStringLiteral("\u2014")) {
		if (!m_meta.isEmpty()) {
			m_meta += QStringLiteral(" · ");
		}
		m_meta += size;
	}

	UpdateTooltip();
	update();
}

void GameCard::LoadCover() {
	if (m_item == nullptr || m_covers == nullptr || !m_cover.isNull()) {
		return;
	}
	m_cover = m_covers->Get(m_item->GetInfo().basedir);
	update();
}

void GameCard::UpdateTooltip() {
	if (m_item == nullptr) {
		return;
	}

	const auto& info = m_item->GetInfo();
	QStringList lines({info.name});
	if (!info.title_id.isEmpty()) {
		lines.append(info.title_id);
	}
	if (!info.gameVersion.isEmpty()) {
		lines.append(QStringLiteral("应用版本 %1").arg(info.gameVersion));
	}
	const auto path = !info.game_path.isEmpty() ? info.game_path : info.basedir;
	if (!path.isEmpty()) {
		lines.append(path);
	}
	setToolTip(lines.join(QLatin1Char('\n')));
}

void GameCard::paintEvent(QPaintEvent* /*event*/) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	const QRectF card(0.5, 0.5, width() - 1.0, height() - 1.0);
	const QColor background = m_selected ? QColor(QStringLiteral("#101827"))
	                                   : (m_hover ? QColor(QStringLiteral("#10151F"))
	                                              : QColor(QStringLiteral("#0E121A")));
	const QColor border = m_selected ? QColor(QStringLiteral("#3D8BFF"))
	                                 : (m_hover ? QColor(QStringLiteral("#2A3344"))
	                                            : QColor(255, 255, 255, 18));

	painter.setPen(QPen(border, m_selected ? 1.4 : 1.0));
	painter.setBrush(background);
	painter.drawRoundedRect(card, 12, 12);

	const QRectF cover_rect(card.left(), card.top(), card.width(), card.height() - kCardInfoHeight);
	DrawCover(&painter, cover_rect, m_cover, m_title, 12);

	painter.setPen(QPen(QColor(QStringLiteral("#141A25")), 1));
	painter.drawLine(QPointF(card.left(), cover_rect.bottom()),
	                 QPointF(card.right(), cover_rect.bottom()));

	const auto status       = m_item != nullptr ? m_item->GetInfo().game_status
	                                            : Configuration::GameStatus::Unknown;
	const auto status_width = LauncherUi::StatusPillWidth(status, 10);
	LauncherUi::PaintStatusPill(&painter,
	                            QRectF(cover_rect.left() + 12, cover_rect.top() + 12, status_width, 22),
	                            status, 10);

	qreal fav_x = cover_rect.right() - 38;
	if (m_running) {
		const auto running_font = LauncherUi::UiFont(9.5, QFont::Medium);
		const auto running_width =
		    QFontMetrics(running_font).horizontalAdvance(QStringLiteral("运行中")) + 22;
		const QRectF running_rect(cover_rect.right() - 12 - running_width, cover_rect.top() + 12,
		                          running_width, 22);
		painter.setPen(Qt::NoPen);
		painter.setBrush(QColor(61, 139, 255, 218));
		painter.drawRoundedRect(running_rect, 6, 6);
		painter.setFont(running_font);
		painter.setPen(QColor(QStringLiteral("#FFFFFF")));
		painter.drawText(running_rect, Qt::AlignCenter, QStringLiteral("运行中"));
		fav_x = running_rect.left() - 34;
	}
	if (m_favorite) {
		const QRectF star_rect(fav_x, cover_rect.top() + 12, 26, 26);
		painter.setPen(Qt::NoPen);
		painter.setBrush(QColor(2, 3, 5, 150));
		painter.drawEllipse(star_rect);
		const auto icon = LauncherUi::TintedIcon(QStringLiteral(":/icons/star-fill.svg"),
		                                         QColor(QStringLiteral("#F2B33D")),
		                                         QSize(14, 14));
		icon.paint(&painter, star_rect.toRect().adjusted(6, 6, -6, -6));
	}

	const qreal text_left  = cover_rect.left() + kCardPadding;
	const qreal text_width = cover_rect.width() - kCardPadding * 2;
	const auto  title_font = LauncherUi::UiFont(13, QFont::DemiBold);
	painter.setFont(title_font);
	painter.setPen(QColor(QStringLiteral("#F2F5FA")));
	painter.drawText(QRectF(text_left, cover_rect.bottom() + 9, text_width, 18),
	                 Qt::AlignLeft | Qt::AlignVCenter,
	                 QFontMetrics(title_font).elidedText(m_title, Qt::ElideRight,
	                                                     static_cast<int>(text_width)));

	const auto meta_font = LauncherUi::MonoFont(10.5, QFont::Normal);
	painter.setFont(meta_font);
	painter.setPen(QColor(QStringLiteral("#9BA4B8")));
	painter.drawText(QRectF(text_left, cover_rect.bottom() + 30, text_width, 15),
	                 Qt::AlignLeft | Qt::AlignVCenter,
	                 QFontMetrics(meta_font).elidedText(m_meta, Qt::ElideRight,
	                                                    static_cast<int>(text_width)));
}

void GameCard::mousePressEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton) {
		emit Clicked(this);
	} else if (event->button() == Qt::RightButton) {
		emit ContextMenuRequested(this, event->globalPosition().toPoint());
	}
	QWidget::mousePressEvent(event);
}

void GameCard::mouseDoubleClickEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton) {
		emit DoubleClicked(this);
	}
	QWidget::mouseDoubleClickEvent(event);
}

void GameCard::enterEvent(QEnterEvent* event) {
	m_hover = true;
	update();
	QWidget::enterEvent(event);
}

void GameCard::leaveEvent(QEvent* event) {
	m_hover = false;
	update();
	QWidget::leaveEvent(event);
}

HeroBanner::HeroBanner(QWidget* parent): QWidget(parent) {
	setFixedHeight(160);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	m_eyebrow = new QLabel(this);
	m_eyebrow->setObjectName(QStringLiteral("HeroEyebrow"));
	m_eyebrow->setAttribute(Qt::WA_TransparentForMouseEvents);

	m_title = new ElidedLabel(this);
	m_title->setObjectName(QStringLiteral("HeroTitle"));
	m_title->setAttribute(Qt::WA_TransparentForMouseEvents);

	m_meta = new QLabel(this);
	m_meta->setObjectName(QStringLiteral("HeroMeta"));
	m_meta->setAttribute(Qt::WA_TransparentForMouseEvents);

	m_play = new QPushButton(this);
	m_play->setObjectName(QStringLiteral("PrimaryButton"));
	m_play->setFixedHeight(38);
	m_play->setMinimumWidth(128);
	m_play->setCursor(Qt::PointingHandCursor);
	m_play->setIcon(LauncherUi::TintedIcon(QStringLiteral(":/icons/play.svg"),
	                                       QColor(Qt::white), QSize(12, 12)));
	m_play->setIconSize(QSize(12, 12));
	LauncherUi::ApplyPrimaryShadow(m_play);

	m_details = new QToolButton(this);
	m_details->setObjectName(QStringLiteral("HeroDetailsButton"));
	m_details->setFixedSize(38, 38);
	m_details->setCursor(Qt::PointingHandCursor);
	m_details->setToolTip(QStringLiteral("查看详情"));
	m_details->setIcon(LauncherUi::TintedIcon(QStringLiteral(":/icons/arrow-right.svg"),
	                                          QColor(QStringLiteral("#F2F5FA")), QSize(16, 16)));
	m_details->setIconSize(QSize(16, 16));

	auto* text_layout = new QVBoxLayout;
	text_layout->setContentsMargins(0, 0, 0, 0);
	text_layout->setSpacing(3);
	text_layout->addStretch(1);
	text_layout->addWidget(m_eyebrow);
	text_layout->addWidget(m_title);
	text_layout->addWidget(m_meta);
	text_layout->addStretch(1);

	auto* button_layout = new QHBoxLayout;
	button_layout->setContentsMargins(0, 0, 0, 0);
	button_layout->setSpacing(10);
	button_layout->addWidget(m_play);
	button_layout->addWidget(m_details);

	auto* layout = new QHBoxLayout(this);
	layout->setContentsMargins(26, 18, 26, 18);
	layout->setSpacing(16);
	layout->addLayout(text_layout, 1);
	layout->addLayout(button_layout, 0);
	layout->setAlignment(button_layout, Qt::AlignVCenter);

	connect(m_play, &QPushButton::clicked, this, &HeroBanner::PlayRequested);
	connect(m_details, &QToolButton::clicked, this, &HeroBanner::DetailsRequested);
}

void HeroBanner::SetItem(ConfigurationItem* item, const QPixmap& cover, const QString& last_played,
                         bool run_enabled) {
	m_item        = item;
	m_cover       = cover;
	m_last_played = last_played;
	m_run_enabled = run_enabled;
	UpdateTexts();
	update();
}

void HeroBanner::SetRunEnabled(bool enabled) {
	if (m_run_enabled == enabled) {
		return;
	}
	m_run_enabled = enabled;
	UpdateTexts();
}

void HeroBanner::Refresh() {
	UpdateTexts();
	update();
}

void HeroBanner::UpdateTexts() {
	if (m_item == nullptr) {
		m_eyebrow->clear();
		m_title->SetFullText({});
		m_meta->clear();
		m_play->setEnabled(false);
		return;
	}

	const auto& info = m_item->GetInfo();
	if (m_last_played.isEmpty()) {
		m_eyebrow->setText(QStringLiteral("最近添加"));
	} else {
		m_eyebrow->setText(QStringLiteral("继续游玩 · 上次运行 %1").arg(m_last_played));
	}
	m_title->SetFullText(info.name);

	QStringList meta;
	if (!info.title_id.isEmpty()) {
		meta.append(info.title_id);
	}
	if (!info.gameVersion.isEmpty()) {
		meta.append(QStringLiteral("应用版本 %1").arg(info.gameVersion));
	}
	const auto size = m_item->text(GAME_SIZE_COLUMN);
	if (!size.isEmpty() && size != QStringLiteral("\u2014")) {
		meta.append(size);
	}
	m_meta->setText(meta.join(QStringLiteral(" · ")));

	const bool running = m_item->IsRunning();
	m_play->setText(running ? QStringLiteral("运行中") : QStringLiteral("开始游戏"));
	m_play->setEnabled(m_run_enabled && !running);
}

void HeroBanner::paintEvent(QPaintEvent* /*event*/) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	const QRectF card(0.5, 0.5, width() - 1.0, height() - 1.0);
	QPainterPath clip;
	clip.addRoundedRect(card, 16, 16);
	painter.setClipPath(clip);

	if (m_cover.isNull()) {
		QLinearGradient background(card.topLeft(), card.bottomRight());
		background.setColorAt(0.0, QColor(QStringLiteral("#0E121A")));
		background.setColorAt(0.55, QColor(QStringLiteral("#101B2E")));
		background.setColorAt(1.0, QColor(QStringLiteral("#0B0E14")));
		painter.fillRect(card, background);

		QRadialGradient glow(QPointF(card.width() * 0.76, card.height() * 0.2),
		                     card.width() * 0.45);
		glow.setColorAt(0.0, QColor(61, 139, 255, 55));
		glow.setColorAt(1.0, QColor(61, 139, 255, 0));
		painter.fillRect(card, glow);
	} else {
		const auto target = card.size().toSize();
		const auto scaled = m_cover.scaled(target, Qt::KeepAspectRatioByExpanding,
		                                   Qt::SmoothTransformation);
		const QRect source((scaled.width() - target.width()) / 2,
		                   (scaled.height() - target.height()) / 2, target.width(),
		                   target.height());
		painter.drawPixmap(card, scaled, QRectF(source));

		QLinearGradient shade(card.left(), 0, card.right(), 0);
		shade.setColorAt(0.0, QColor(7, 9, 13, 246));
		shade.setColorAt(0.45, QColor(7, 9, 13, 180));
		shade.setColorAt(0.78, QColor(7, 9, 13, 60));
		shade.setColorAt(1.0, QColor(7, 9, 13, 24));
		painter.fillRect(card, shade);

		QLinearGradient bottom(0, card.bottom() - 70, 0, card.bottom());
		bottom.setColorAt(0.0, QColor(7, 9, 13, 0));
		bottom.setColorAt(1.0, QColor(7, 9, 13, 150));
		painter.fillRect(card, bottom);

		QRadialGradient glow(QPointF(card.width() * 0.82, card.height() * 0.22),
		                     card.width() * 0.34);
		glow.setColorAt(0.0, QColor(61, 139, 255, 30));
		glow.setColorAt(1.0, QColor(61, 139, 255, 0));
		painter.fillRect(card, glow);
	}

	painter.setClipping(false);
	painter.setPen(QPen(QColor(QStringLiteral("#1E2431")), 1));
	painter.setBrush(Qt::NoBrush);
	painter.drawRoundedRect(card, 16, 16);
}
