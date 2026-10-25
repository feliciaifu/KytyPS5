#include "libraryPage.h"

#include "configurationItem.h"
#include "configurationListWidget.h"
#include "coverLoader.h"
#include "gameCards.h"
#include "gameContent.h"
#include "gameViews.h"
#include "launcherUiUtils.h"
#include "trophyViewerDialog.h"

#include <algorithm>

#include <QAbstractItemModel>
#include <QVersionNumber>
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QButtonGroup>
#include <QCursor>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kGridGap        = 16;
constexpr int kDetailsWidth   = 320;
constexpr int kCardInfoHeight = 53;

QString NormalizePath(QString path) {
	path = QDir::cleanPath(path.trimmed());
#ifdef _WIN32
	path = path.toCaseFolded();
#endif
	return path;
}

bool PathUnder(const QString& path, const QString& directory) {
	if (directory.isEmpty()) {
		return true;
	}
	auto normalized_path = NormalizePath(path);
	auto normalized_dir  = NormalizePath(directory);
	if (normalized_path == normalized_dir) {
		return true;
	}
	if (!normalized_dir.endsWith(QLatin1Char('/'))) {
		normalized_dir += QLatin1Char('/');
	}
	return normalized_path.startsWith(normalized_dir);
}

class RowIndicatorDelegate: public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;

	void paint(QPainter* painter, const QStyleOptionViewItem& option,
	           const QModelIndex& index) const override {
		QStyledItemDelegate::paint(painter, option, index);
		if (option.state.testFlag(QStyle::State_Selected)) {
			painter->save();
			painter->fillRect(QRectF(option.rect.left(), option.rect.top() + 16, 2, 20),
			                  QColor(QStringLiteral("#3D8BFF")));
			painter->restore();
		}
	}
};
QPushButton* MakeIconButton(const QString& object_name, const QString& icon, const QColor& color,
                            int size, QWidget* parent) {
	auto* button = new QPushButton(parent);
	button->setObjectName(object_name);
	button->setFixedSize(size, size);
	button->setFlat(true);
	button->setCursor(Qt::PointingHandCursor);
	button->setIcon(LauncherUi::TintedIcon(icon, color, QSize(size - 14, size - 14)));
	button->setIconSize(QSize(size - 14, size - 14));
	return button;
}

} // namespace

LibraryPage::LibraryPage(ConfigurationListWidget* controller, CoverLoader* covers, QWidget* parent)
    : QWidget(parent), m_controller(controller), m_covers(covers) {
	m_last_scan = QDateTime::currentDateTime();
	BuildUi();

	connect(m_controller, &ConfigurationListWidget::ItemsChanged, this,
	        &LibraryPage::RefreshItems);
	connect(m_controller, &ConfigurationListWidget::SelectionChanged, this,
	        &LibraryPage::RefreshSelection);
	connect(m_controller->GetTreeWidget()->model(), &QAbstractItemModel::dataChanged, this,
	        [this]() { RefreshItemData(); });
	connect(m_controller->GetTreeWidget()->model(), &QAbstractItemModel::rowsAboutToBeRemoved,
	        this, &LibraryPage::ClearForRescan);

	RefreshItems();
}

void LibraryPage::BuildUi() {
	m_root = new QVBoxLayout(this);
	m_root->setContentsMargins(32, 24, 32, 24);
	m_root->setSpacing(16);

	BuildHeader();
	BuildFilterBar();

	m_content = new QStackedWidget(this);
	m_content->setObjectName(QStringLiteral("LibraryContent"));

	BuildGridPage();
	BuildListPage();
	BuildEmptyPage();

	m_content->addWidget(m_grid_page);
	m_content->addWidget(m_list_page);
	m_content->addWidget(m_empty_page);
	m_root->addWidget(m_content, 1);
}

void LibraryPage::BuildHeader() {
	m_title    = new QLabel(this);
	m_subtitle = new QLabel(this);
	m_title->setObjectName(QStringLiteral("PageTitle"));
	m_subtitle->setObjectName(QStringLiteral("PageSubtitle"));

	auto* title_layout = new QVBoxLayout;
	title_layout->setContentsMargins(0, 0, 0, 0);
	title_layout->setSpacing(3);
	title_layout->addWidget(m_title);
	title_layout->addWidget(m_subtitle);

	m_search = new QLineEdit(this);
	m_search->setObjectName(QStringLiteral("SearchBox"));
	m_search->setFixedSize(264, 36);
	m_search->setPlaceholderText(QStringLiteral("搜索游戏名称或 Serial"));
	m_search->setClearButtonEnabled(true);
	m_search->addAction(LauncherUi::TintedIcon(QStringLiteral(":/icons/search.svg"),
	                                           QColor(QStringLiteral("#5F6A80")), QSize(16, 16)),
	                    QLineEdit::LeadingPosition);
	m_search->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	m_search_timer = new QTimer(this);
	m_search_timer->setSingleShot(true);
	m_search_timer->setInterval(140);
	connect(m_search_timer, &QTimer::timeout, this, &LibraryPage::RebuildViews);
	connect(m_search, &QLineEdit::textChanged, this, [this]() { m_search_timer->start(); });

	m_view_toggle = new QFrame(this);
	m_view_toggle->setObjectName(QStringLiteral("ViewToggle"));
	m_view_toggle->setFixedSize(68, 36);
	auto* toggle_layout = new QHBoxLayout(m_view_toggle);
	toggle_layout->setContentsMargins(3, 4, 3, 4);
	toggle_layout->setSpacing(2);

	m_grid_button = new QToolButton(m_view_toggle);
	m_list_button = new QToolButton(m_view_toggle);
	for (auto* button: {m_grid_button, m_list_button}) {
		button->setObjectName(QStringLiteral("ViewToggleButton"));
		button->setCheckable(true);
		button->setAutoRaise(true);
		button->setFixedSize(30, 28);
		button->setCursor(Qt::PointingHandCursor);
		toggle_layout->addWidget(button);
	}
	m_grid_button->setIcon(LauncherUi::TintedIcon(QStringLiteral(":/icons/grid-view.svg"),
	                                              QColor(QStringLiteral("#9BA4B8")), QSize(16, 16)));
	m_list_button->setIcon(LauncherUi::TintedIcon(QStringLiteral(":/icons/list.svg"),
	                                              QColor(QStringLiteral("#9BA4B8")), QSize(16, 16)));
	m_grid_button->setIconSize(QSize(16, 16));
	m_list_button->setIconSize(QSize(16, 16));
	m_grid_button->setToolTip(QStringLiteral("网格视图"));
	m_list_button->setToolTip(QStringLiteral("列表视图"));

	auto* view_group = new QButtonGroup(this);
	view_group->setExclusive(true);
	view_group->addButton(m_grid_button);
	view_group->addButton(m_list_button);
	connect(m_grid_button, &QToolButton::clicked, this, [this]() { SetListMode(false); });
	connect(m_list_button, &QToolButton::clicked, this, [this]() { SetListMode(true); });

	m_more_button = MakeIconButton(QStringLiteral("MoreButton"), QStringLiteral(":/icons/more.svg"),
	                               QColor(QStringLiteral("#9BA4B8")), 36, this);
	connect(m_more_button, &QPushButton::clicked, this, &LibraryPage::ShowHeaderMenu);

	auto* header = new QHBoxLayout;
	header->setContentsMargins(0, 0, 0, 0);
	header->setSpacing(10);
	header->addLayout(title_layout, 1);
	header->addWidget(m_search, 0, Qt::AlignVCenter);
	header->addWidget(m_view_toggle, 0, Qt::AlignVCenter);
	header->addWidget(m_more_button, 0, Qt::AlignVCenter);
	m_root->addLayout(header);
}

void LibraryPage::BuildFilterBar() {
	auto* bar = new QHBoxLayout;
	bar->setContentsMargins(0, 0, 0, 0);
	bar->setSpacing(8);

	auto* group = new QButtonGroup(this);
	group->setExclusive(true);

	const struct {
		int     status;
		QString label;
	} filters[] = {
	    {-1, QStringLiteral("全部")},
	    {static_cast<int>(Configuration::GameStatus::InGame), QStringLiteral("可玩")},
	    {static_cast<int>(Configuration::GameStatus::MainMenu), QStringLiteral("主菜单")},
	    {static_cast<int>(Configuration::GameStatus::Logo), QStringLiteral("卡 Logo")},
	    {static_cast<int>(Configuration::GameStatus::DoesntBoot), QStringLiteral("无法启动")},
	    {static_cast<int>(Configuration::GameStatus::Unknown), QStringLiteral("未测试")},
	};

	for (const auto& filter: filters) {
		auto* button = new QPushButton(filter.label, this);
		button->setObjectName(QStringLiteral("FilterChip"));
		button->setCheckable(true);
		button->setCursor(Qt::PointingHandCursor);
		button->setFixedHeight(28);
		group->addButton(button);
		bar->addWidget(button);
		m_chips.append({button, filter.status});

		connect(button, &QPushButton::clicked, this, [this, status = filter.status]() {
			m_status_filter = status;
			RebuildViews();
		});
	}

	m_chips.first().button->setChecked(true);

	bar->addStretch(1);
	m_sort_button = new QPushButton(this);
	m_sort_button->setObjectName(QStringLiteral("SortButton"));
	m_sort_button->setFlat(true);
	m_sort_button->setCursor(Qt::PointingHandCursor);
	m_sort_button->setVisible(false);
	connect(m_sort_button, &QPushButton::clicked, this, &LibraryPage::ShowSortMenu);
	bar->addWidget(m_sort_button);
	UpdateSortButton();

	auto* container = new QWidget(this);
	container->setLayout(bar);
	m_root->addWidget(container);
}

void LibraryPage::BuildGridPage() {
	m_grid_page = new QWidget(this);
	auto* layout = new QVBoxLayout(m_grid_page);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(kGridGap);

	m_hero = new HeroBanner(m_grid_page);
	layout->addWidget(m_hero);
	connect(m_hero, &HeroBanner::PlayRequested, this, [this]() { PlayItem(HeroItem()); });
	connect(m_hero, &HeroBanner::DetailsRequested, this, [this]() {
		SetListMode(true);
		SelectItem(HeroItem(), true);
	});

	m_scroll = new QScrollArea(m_grid_page);
	m_scroll->setObjectName(QStringLiteral("ContentScroll"));
	m_scroll->setFrameShape(QFrame::NoFrame);
	m_scroll->setWidgetResizable(true);
	m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_scroll->viewport()->installEventFilter(this);

	m_cards_host = new QWidget(m_scroll);
	m_cards_host->setObjectName(QStringLiteral("Transparent"));
	m_cards_layout = new QGridLayout(m_cards_host);
	m_cards_layout->setContentsMargins(0, 0, 0, 0);
	m_cards_layout->setSpacing(kGridGap);
	m_cards_layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_scroll->setWidget(m_cards_host);
	layout->addWidget(m_scroll, 1);
}

void LibraryPage::BuildListPage() {
	m_list_page = new QWidget(this);
	auto* layout = new QHBoxLayout(m_list_page);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(kGridGap);

	auto* table_card = new QFrame(m_list_page);
	table_card->setObjectName(QStringLiteral("TableCard"));
	auto* table_layout = new QVBoxLayout(table_card);
	table_layout->setContentsMargins(0, 0, 0, 0);
	table_layout->setSpacing(0);

	m_table = new QTableWidget(table_card);
	m_table->setObjectName(QStringLiteral("GameTable"));
	m_table->setColumnCount(4);
	m_table->setHorizontalHeaderLabels(
	    {QStringLiteral("游戏"), QStringLiteral("SERIAL"), QStringLiteral("大小"),
	     QStringLiteral("状态")});
	m_table->setShowGrid(false);
	m_table->setFrameShape(QFrame::NoFrame);
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->setAlternatingRowColors(false);
	m_table->setWordWrap(false);
	m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_table->verticalHeader()->setVisible(false);
	m_table->verticalHeader()->setDefaultSectionSize(52);
	m_table->horizontalHeader()->setFixedHeight(36);
	m_table->horizontalHeader()->setHighlightSections(false);
	m_table->horizontalHeader()->setStretchLastSection(false);
	m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	m_table->setItemDelegateForColumn(0, new RowIndicatorDelegate(m_table));
	m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
	m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
	m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
	m_table->setColumnWidth(1, 92);
	m_table->setColumnWidth(2, 68);
	m_table->setColumnWidth(3, 80);
	m_table->setContextMenuPolicy(Qt::CustomContextMenu);
	table_layout->addWidget(m_table, 1);

	auto* footer = new QWidget(table_card);
	footer->setObjectName(QStringLiteral("TableFooter"));
	footer->setFixedHeight(36);
	auto* footer_layout = new QHBoxLayout(footer);
	footer_layout->setContentsMargins(14, 0, 14, 0);
	footer_layout->setSpacing(8);
	m_table_footer = new QLabel(footer);
	m_table_footer->setObjectName(QStringLiteral("FooterText"));
	footer_layout->addWidget(m_table_footer, 1);
	auto* rescan = new QPushButton(QStringLiteral("重新扫描"), footer);
	rescan->setObjectName(QStringLiteral("LinkButton"));
	rescan->setCursor(Qt::PointingHandCursor);
	rescan->setFlat(true);
	rescan->setFixedHeight(24);
	footer_layout->addWidget(rescan);
	connect(rescan, &QPushButton::clicked, this, &LibraryPage::Rescan);
	table_layout->addWidget(footer);

	layout->addWidget(table_card, 1);

	m_details = new QFrame(m_list_page);
	m_details->setObjectName(QStringLiteral("DetailsPanel"));
	m_details->setFixedWidth(kDetailsWidth);
	auto* details_layout = new QVBoxLayout(m_details);
	details_layout->setContentsMargins(16, 16, 16, 16);
	details_layout->setSpacing(0);
	m_details_stack = new QStackedWidget(m_details);
	details_layout->addWidget(m_details_stack);
	BuildDetailsPanel();

	layout->addWidget(m_details, 0);

	connect(m_table, &QTableWidget::currentCellChanged, this,
	        [this](int row, int /*column*/, int /*prev_row*/, int /*prev_column*/) {
		        OnTableCurrentChanged(row, 0);
	        });
	connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int /*column*/) {
		PlayItem(ItemForRow(row));
	});
	connect(m_table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
		const auto row = m_table->rowAt(pos.y());
		ShowGameMenu(ItemForRow(row), m_table->viewport()->mapToGlobal(pos));
	});
}

void LibraryPage::BuildDetailsPanel() {
	auto* content = new QWidget;
	content->setObjectName(QStringLiteral("Transparent"));
	auto* layout = new QVBoxLayout(content);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(10);

	m_detail_cover = new CoverView(content);
	m_detail_cover->setFixedSize(288, 162);
	m_detail_cover->SetCornerRadius(10);
	layout->addWidget(m_detail_cover);

	auto* title_row = new QHBoxLayout;
	title_row->setContentsMargins(0, 0, 0, 0);
	title_row->setSpacing(6);
	m_detail_title = new ElidedLabel(content);
	m_detail_title->setObjectName(QStringLiteral("DetailTitle"));
	title_row->addWidget(m_detail_title, 1);
	m_detail_favorite = new QToolButton(content);
	m_detail_favorite->setObjectName(QStringLiteral("IconButton"));
	m_detail_favorite->setFixedSize(32, 32);
	m_detail_favorite->setCursor(Qt::PointingHandCursor);
	m_detail_favorite->setToolTip(QStringLiteral("收藏"));
	title_row->addWidget(m_detail_favorite, 0, Qt::AlignTop);
	m_detail_more = new QToolButton(content);
	m_detail_more->setObjectName(QStringLiteral("IconButton"));
	m_detail_more->setFixedSize(32, 32);
	m_detail_more->setCursor(Qt::PointingHandCursor);
	m_detail_more->setIcon(LauncherUi::TintedIcon(QStringLiteral(":/icons/more.svg"),
	                                              QColor(QStringLiteral("#9BA4B8")), QSize(14, 14)));
	m_detail_more->setIconSize(QSize(14, 14));
	m_detail_more->setToolTip(QStringLiteral("更多操作"));
	title_row->addWidget(m_detail_more, 0, Qt::AlignTop);
	layout->addLayout(title_row);

	m_detail_meta = new QLabel(content);
	m_detail_meta->setObjectName(QStringLiteral("DetailMeta"));
	layout->addWidget(m_detail_meta);

	auto* status_row = new QHBoxLayout;
	status_row->setContentsMargins(0, 2, 0, 2);
	status_row->setSpacing(8);
	m_detail_status = new StatusPill(content);
	status_row->addWidget(m_detail_status);
	status_row->addStretch(1);
	m_detail_community = new QLabel(content);
	m_detail_community->setObjectName(QStringLiteral("DetailMeta"));
	status_row->addWidget(m_detail_community);
	m_detail_compat_edit = new QPushButton(QStringLiteral("编辑兼容性"), content);
	m_detail_compat_edit->setObjectName(QStringLiteral("LinkButton"));
	m_detail_compat_edit->setFlat(true);
	m_detail_compat_edit->setFixedHeight(20);
	m_detail_compat_edit->setCursor(Qt::PointingHandCursor);
	m_detail_compat_edit->setVisible(false);
	status_row->addWidget(m_detail_compat_edit);
	connect(m_detail_compat_edit, &QPushButton::clicked, this,
	        &LibraryPage::ShowCompatibilityEditor);
	layout->addLayout(status_row);

	auto* info = new QGridLayout;
	info->setContentsMargins(0, 4, 0, 4);
	info->setHorizontalSpacing(10);
	info->setVerticalSpacing(7);
	info->setColumnStretch(1, 1);

	const auto add_info_row = [this, info](int row, const QString& label, QLabel** value) {
		auto* name = new QLabel(label, this);
		name->setObjectName(QStringLiteral("InfoLabel"));
		*value = new QLabel(QStringLiteral("\u2014"), this);
		(*value)->setObjectName(QStringLiteral("InfoValue"));
		(*value)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		info->addWidget(name, row, 0);
		info->addWidget(*value, row, 1);
	};
	add_info_row(0, QStringLiteral("固件版本"), &m_detail_firmware);
	add_info_row(1, QStringLiteral("游戏大小"), &m_detail_size);
	add_info_row(2, QStringLiteral("安装路径"), &m_detail_path);
	add_info_row(3, QStringLiteral("最后运行"), &m_detail_last_run);
	for (auto* label: {m_detail_firmware, m_detail_size, m_detail_last_run}) {
		label->setFont(LauncherUi::MonoFont(11, QFont::Medium));
	}
	layout->addLayout(info);

	m_detail_play = new QPushButton(QStringLiteral("开始游戏"), content);
	m_detail_play->setObjectName(QStringLiteral("PrimaryButton"));
	m_detail_play->setFixedHeight(38);
	m_detail_play->setCursor(Qt::PointingHandCursor);
	m_detail_play->setIcon(LauncherUi::TintedIcon(QStringLiteral(":/icons/play.svg"),
	                                               QColor(Qt::white), QSize(12, 12)));
	m_detail_play->setIconSize(QSize(12, 12));
	LauncherUi::ApplyPrimaryShadow(m_detail_play);
	layout->addWidget(m_detail_play);

	auto* secondary = new QHBoxLayout;
	secondary->setContentsMargins(0, 0, 0, 0);
	secondary->setSpacing(8);
	m_detail_settings = new QPushButton(QStringLiteral("游戏设置"), content);
	m_detail_settings->setObjectName(QStringLiteral("SecondaryButton"));
	m_detail_settings->setFixedHeight(34);
	m_detail_settings->setCursor(Qt::PointingHandCursor);
	m_detail_input = new QPushButton(QStringLiteral("输入映射"), content);
	m_detail_input->setObjectName(QStringLiteral("SecondaryButton"));
	m_detail_input->setFixedHeight(34);
	m_detail_input->setCursor(Qt::PointingHandCursor);
	secondary->addWidget(m_detail_settings, 1);
	secondary->addWidget(m_detail_input, 1);
	layout->addLayout(secondary);

	auto* separator = new QFrame(content);
	separator->setFrameShape(QFrame::HLine);
	separator->setFrameShadow(QFrame::Plain);
	layout->addWidget(separator);

	auto* trophy_row = new QHBoxLayout;
	trophy_row->setContentsMargins(0, 2, 0, 0);
	auto* trophy_label = new QLabel(QStringLiteral("奖杯进度"), content);
	trophy_label->setObjectName(QStringLiteral("InfoLabel"));
	m_detail_trophy_value = new QLabel(QStringLiteral("\u2014"), content);
	m_detail_trophy_value->setObjectName(QStringLiteral("InfoValue"));
	trophy_row->addWidget(trophy_label);
	trophy_row->addStretch(1);
	trophy_row->addWidget(m_detail_trophy_value);
	layout->addLayout(trophy_row);

	auto* trophy_bar = new QProgressBar(content);
	trophy_bar->setObjectName(QStringLiteral("TrophyBar"));
	trophy_bar->setRange(0, 100);
	trophy_bar->setValue(0);
	trophy_bar->setTextVisible(false);
	layout->addWidget(trophy_bar);

	m_detail_trophy_note = new QLabel(content);
	m_detail_trophy_note->setObjectName(QStringLiteral("DetailNote"));
	m_detail_trophy_note->setWordWrap(true);
	layout->addWidget(m_detail_trophy_note);

	m_detail_trophy = new QPushButton(QStringLiteral("查看本地奖杯"), content);
	m_detail_trophy->setObjectName(QStringLiteral("SecondaryButton"));
	m_detail_trophy->setFixedHeight(34);
	m_detail_trophy->setCursor(Qt::PointingHandCursor);
	layout->addWidget(m_detail_trophy);

	layout->addStretch(1);

	m_detail_note = new QLabel(content);
	m_detail_note->setObjectName(QStringLiteral("DetailNote"));
	m_detail_note->setWordWrap(true);
	layout->addWidget(m_detail_note);

	auto* scroll = new QScrollArea(m_details_stack);
	scroll->setObjectName(QStringLiteral("DetailsScroll"));
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setWidget(content);
	m_details_stack->addWidget(scroll);

	auto* empty = new QWidget(m_details_stack);
	auto* empty_layout = new QVBoxLayout(empty);
	auto* empty_label = new QLabel(QStringLiteral("选择一个游戏查看详情"), empty);
	empty_label->setObjectName(QStringLiteral("DetailEmpty"));
	empty_label->setAlignment(Qt::AlignCenter);
	empty_layout->addStretch(1);
	empty_layout->addWidget(empty_label);
	empty_layout->addStretch(1);
	m_details_stack->addWidget(empty);
	m_detail_empty = empty;

	connect(m_detail_play, &QPushButton::clicked, this, [this]() { PlayItem(m_selected); });
	connect(m_detail_settings, &QPushButton::clicked, this, [this]() {
		if (m_selected != nullptr) {
			m_controller->EditSelectedGame();
		}
	});
	connect(m_detail_input, &QPushButton::clicked, this,
	        [this]() { m_controller->EditInputMapping(); });
	connect(m_detail_favorite, &QToolButton::clicked, this, [this]() {
		if (m_selected != nullptr) {
			ToggleFavorite(m_selected);
		}
	});
	connect(m_detail_more, &QToolButton::clicked, this, [this]() {
		if (m_selected != nullptr) {
			ShowGameMenu(m_selected, QCursor::pos());
		}
	});
	connect(m_detail_trophy, &QPushButton::clicked, this,
	        [this]() { m_controller->ViewTrophies(); });
}

void LibraryPage::BuildEmptyPage() {
	m_empty_page = new QWidget(this);
	auto* root = new QVBoxLayout(m_empty_page);
	root->setContentsMargins(0, 0, 0, 0);
	root->addStretch(1);

	auto* center = new QHBoxLayout;
	center->addStretch(1);
	auto* card = new QFrame(m_empty_page);
	card->setObjectName(QStringLiteral("EmptyCard"));
	card->setFixedSize(440, 200);
	auto* card_layout = new QVBoxLayout(card);
	card_layout->setContentsMargins(18, 18, 18, 18);
	card_layout->setSpacing(6);

	m_empty_icon = new QLabel(card);
	m_empty_icon->setAlignment(Qt::AlignCenter);
	m_empty_icon->setFixedHeight(40);

	m_empty_title = new QLabel(card);
	m_empty_title->setObjectName(QStringLiteral("EmptyTitle"));
	m_empty_title->setAlignment(Qt::AlignCenter);

	m_empty_text = new QLabel(card);
	m_empty_text->setObjectName(QStringLiteral("EmptyText"));
	m_empty_text->setAlignment(Qt::AlignCenter);
	m_empty_text->setWordWrap(true);

	m_empty_button = new QPushButton(card);
	m_empty_button->setObjectName(QStringLiteral("PrimaryButton"));
	m_empty_button->setFixedHeight(38);
	m_empty_button->setCursor(Qt::PointingHandCursor);

	card_layout->addWidget(m_empty_icon);
	card_layout->addWidget(m_empty_title);
	card_layout->addWidget(m_empty_text);
	card_layout->addStretch(1);
	LauncherUi::ApplyPrimaryShadow(m_empty_button);
	card_layout->addWidget(m_empty_button);

	center->addWidget(card);
	center->addStretch(1);
	root->addLayout(center);
	root->addStretch(1);

	connect(m_empty_button, &QPushButton::clicked, this, [this]() {
		if (m_mode == Mode::Favorites || m_mode == Mode::Compatibility) {
			emit RequestLibraryMode();
		} else {
			AddGameDirectory();
		}
	});
}

void LibraryPage::SetMode(Mode mode) {
	if (m_mode == mode) {
		return;
	}
	m_mode = mode;
	if (mode == Mode::Compatibility) {
		SetListMode(true);
	}
	UpdateHeader();
	RebuildViews();
}

void LibraryPage::SetFavorites(const QSet<QString>& favorites) {
	if (m_favorites == favorites) {
		return;
	}
	m_favorites = favorites;
	for (auto* card: m_cards) {
		card->SetFavorite(m_favorites.contains(FavoriteKey(card->GetItem())));
	}
	UpdateDetails();
	if (m_mode == Mode::Favorites) {
		RebuildViews();
	}
}

void LibraryPage::SetLastPlayed(const QString& path, const QDateTime& time) {
	m_last_played_path = path;
	m_last_played_time = time;
	UpdateHero();
	UpdateDetails();
}

void LibraryPage::SetRunEnabled(bool enabled) {
	if (m_run_enabled == enabled) {
		return;
	}
	m_run_enabled = enabled;
	UpdateHero();
	UpdateDetails();
}

void LibraryPage::SetDirectoryFilter(const QString& directory) {
	m_directory_filter = directory;
	RebuildViews();
}

void LibraryPage::SetListMode(bool list_mode) {
	if (m_list_mode == list_mode) {
		UpdateViewButtons();
		return;
	}
	m_list_mode = list_mode;
	UpdateViewButtons();
	UpdateEmptyState();
	UpdateHero();
	if (m_list_mode && m_selected != nullptr) {
		SelectItem(m_selected, true);
	}
	emit ViewModeChanged(m_list_mode);
}

void LibraryPage::SetGridColumns(int columns) {
	columns = qBound(0, columns, 5);
	if (m_grid_columns == columns) {
		return;
	}
	m_grid_columns = columns;
	if (m_cards_layout != nullptr && m_scroll != nullptr) {
		RelayoutCards();
	}
}

bool LibraryPage::eventFilter(QObject* watched, QEvent* event) {
	if (watched == m_scroll->viewport() && event->type() == QEvent::Resize) {
		RelayoutCards();
	}
	return QWidget::eventFilter(watched, event);
}

void LibraryPage::ResetUiState() {
	{
		const QSignalBlocker blocker(m_search);
		m_search->clear();
	}
	m_status_filter = -1;
	if (m_scroll != nullptr) {
		m_scroll->verticalScrollBar()->setValue(0);
	}
	RebuildViews();
}

void LibraryPage::RefreshItems() {
	m_items = m_controller->GetItems();
	SortItems(m_items);
	m_last_scan = QDateTime::currentDateTime();
	RebuildViews();
	RefreshSelection(m_controller->GetSelectedItem());
	if (m_selected == nullptr && !m_visible_items.isEmpty()) {
		SelectItem(m_visible_items.first());
	}
}

void LibraryPage::RefreshSelection(ConfigurationItem* item) {
	m_selected = item;
	for (auto* card: m_cards) {
		card->SetSelected(card->GetItem() == item);
	}
	if (m_table != nullptr) {
		const QSignalBlocker blocker(m_table);
		m_syncing = true;
		if (item == nullptr) {
			m_table->clearSelection();
			m_table->setCurrentCell(-1, -1);
		} else {
			const int row = RowForItem(item);
			if (row >= 0) {
				m_table->setCurrentCell(row, 0);
			}
		}
		m_syncing = false;
	}
	UpdateDetails();
	UpdateHero();
}

void LibraryPage::ClearForRescan() {
	if (m_items.isEmpty() && m_cards.isEmpty() && m_table->rowCount() == 0) {
		return;
	}
	m_selected = nullptr;
	m_items.clear();
	m_visible_items.clear();
	for (auto* card: m_cards) {
		card->hide();
		card->deleteLater();
	}
	m_cards.clear();
	while (auto* item = m_cards_layout->takeAt(0)) {
		delete item;
	}
	if (m_table != nullptr) {
		const QSignalBlocker blocker(m_table);
		m_syncing = true;
		m_table->setRowCount(0);
		m_syncing = false;
	}
	if (m_details_stack != nullptr) {
		m_details_stack->setCurrentIndex(1);
	}
}

void LibraryPage::RefreshItemData() {
	for (auto* card: m_cards) {
		card->Refresh();
		card->SetFavorite(m_favorites.contains(FavoriteKey(card->GetItem())));
	}
	for (int row = 0; row < m_table->rowCount(); row++) {
		UpdateTableRow(row, ItemForRow(row));
	}
	UpdateChips();
	UpdateHeader();
	UpdateHero();
	UpdateDetails();
}

void LibraryPage::RebuildViews() {
	m_visible_items = VisibleItems();
	RebuildCards(m_visible_items);
	RebuildTable(m_visible_items);
	UpdateHeader();
	UpdateChips();
	UpdateEmptyState();
	UpdateHero();
	UpdateDetails();
	UpdateViewButtons();
}

void LibraryPage::RebuildCards(const QVector<ConfigurationItem*>& items) {
	for (auto* card: m_cards) {
		card->hide();
		card->deleteLater();
	}
	m_cards.clear();

	while (auto* item = m_cards_layout->takeAt(0)) {
		delete item;
	}

	for (auto* item: items) {
		auto* card = new GameCard(item, m_covers, m_cards_host);
		card->SetFavorite(m_favorites.contains(FavoriteKey(item)));
		card->SetSelected(item == m_selected);
		connect(card, &GameCard::Clicked, this, &LibraryPage::OnCardClicked);
		connect(card, &GameCard::DoubleClicked, this, &LibraryPage::OnCardDoubleClicked);
		connect(card, &GameCard::ContextMenuRequested, this,
		        &LibraryPage::OnCardContextMenu);
		m_cards.append(card);
	}

	RelayoutCards();
	ScheduleCoverLoads();
}

void LibraryPage::RebuildTable(const QVector<ConfigurationItem*>& items) {
	const QSignalBlocker blocker(m_table);
	const int          count = static_cast<int>(items.size());
	m_syncing = true;
	m_table->setRowCount(0);
	m_table->setRowCount(count);

	for (int row = 0; row < count; row++) {
		auto* item = items.at(row);
		auto* name = new QTableWidgetItem;
		m_table->setItem(row, 0, name);
		m_table->setItem(row, 1, new QTableWidgetItem);
		m_table->setItem(row, 2, new QTableWidgetItem);
		m_table->setItem(row, 3, new QTableWidgetItem);


		UpdateTableRow(row, item);
	}
	m_syncing = false;

	if (m_selected != nullptr) {
		const int row = RowForItem(m_selected);
		if (row >= 0) {
			m_syncing = true;
			m_table->setCurrentCell(row, 0);
			m_syncing = false;
		}
	}
}

void LibraryPage::UpdateTableRow(int row, ConfigurationItem* item) {
	if (item == nullptr || row < 0 || row >= m_table->rowCount()) {
		return;
	}
	const auto& info = item->GetInfo();
	if (auto* name = m_table->item(row, 0); name != nullptr) {
		name->setText(info.name);
		name->setFont(LauncherUi::UiFont(12.5, QFont::Medium));
		name->setForeground(QColor(QStringLiteral("#DDE3EF")));
		name->setData(Qt::UserRole,
		              QVariant::fromValue<qulonglong>(
		                  static_cast<qulonglong>(reinterpret_cast<quintptr>(item))));
		name->setToolTip(info.name);
	}
	if (auto* serial = m_table->item(row, 1); serial != nullptr) {
		serial->setText(info.title_id);
		serial->setFont(LauncherUi::MonoFont(10.5, QFont::Medium));
		serial->setForeground(QColor(QStringLiteral("#9BA4B8")));
	}
	if (auto* size = m_table->item(row, 2); size != nullptr) {
		size->setText(item->text(GAME_SIZE_COLUMN));
		size->setFont(LauncherUi::MonoFont(10.5, QFont::Medium));
		size->setForeground(QColor(QStringLiteral("#9BA4B8")));
	}
	if (auto* status = m_table->item(row, 3); status != nullptr) {
		status->setText(LauncherUi::StatusText(info.game_status));
		status->setFont(LauncherUi::UiFont(11, QFont::Medium));
		status->setForeground(LauncherUi::StatusColor(info.game_status));
		status->setToolTip(info.game_comment);
	}
}

void LibraryPage::RelayoutCards() {
	while (auto* item = m_cards_layout->takeAt(0)) {
		delete item;
	}

	const int available = m_scroll->viewport()->width();
	int       columns   = m_grid_columns;
	if (columns <= 0) {
		columns = available >= 780 ? 3 : (available >= 520 ? 2 : 1);
	}
	constexpr int kMinCardWidth = 180;
	const int max_columns =
	    qMax(1, (available + kGridGap) / (kMinCardWidth + kGridGap));
	columns = qBound(1, qMin(columns, max_columns), 5);
	const int card_width =
	    qMax(1, (available - kGridGap * (columns - 1)) / columns);
	const int card_height = qRound(card_width * 9.0 / 16.0) + kCardInfoHeight;

	for (auto* card: m_cards) {
		card->setFixedHeight(card_height);
	}

	for (int column = 0; column < 5; column++) {
		m_cards_layout->setColumnStretch(column, column < columns ? 1 : 0);
	}

	for (int index = 0; index < m_cards.size(); index++) {
		m_cards_layout->addWidget(m_cards.at(index), index / columns, index % columns);
	}
	m_cards_host->updateGeometry();
}

void LibraryPage::UpdateHeader() {
	const int base_count = BaseItems().size();
	const int dirs       = m_controller->GetGameDirectories().size();
	const auto last      = LauncherUi::RelativeTime(m_last_scan);

	QString title;
	QString subtitle;
	switch (m_mode) {
		case Mode::Library:
			title = QStringLiteral("游戏库");
			subtitle = QStringLiteral("%1 个游戏 · %2 个游戏目录 · %3扫描")
			               .arg(base_count)
			               .arg(dirs)
			               .arg(last.isEmpty() ? QStringLiteral("刚刚") : last);
			break;
		case Mode::Favorites:
			title    = QStringLiteral("收藏夹");
			subtitle = QStringLiteral("%1 款收藏 · 共 %2 个游戏").arg(base_count).arg(m_items.size());
			break;
		case Mode::Compatibility:
			title    = QStringLiteral("兼容性列表");
			subtitle = QStringLiteral("社区兼容性数据 · %1 个游戏").arg(base_count);
			break;
	}
	if (!m_directory_filter.isEmpty()) {
		subtitle += QStringLiteral(" · 目录：%1").arg(QFileInfo(m_directory_filter).fileName());
	}
	m_title->setText(title);
	m_subtitle->setText(subtitle);

	if (m_table_footer != nullptr) {
		m_table_footer->setText(QStringLiteral("已检测 %1 个游戏 · 上次扫描 %2")
		                            .arg(base_count)
		                            .arg(last.isEmpty() ? QStringLiteral("刚刚") : last));
	}
}

void LibraryPage::UpdateChips() {
	const auto base = BaseItems();
	for (auto& chip: m_chips) {
		int count = 0;
		for (auto* item: base) {
			if (chip.status < 0 ||
			    static_cast<int>(item->GetInfo().game_status) == chip.status) {
				count++;
			}
		}
		const auto label =
		    chip.status < 0
		        ? QStringLiteral("全部")
		        : LauncherUi::StatusText(static_cast<Configuration::GameStatus>(chip.status));
		chip.button->setText(QStringLiteral("%1 %2").arg(label).arg(count));
		chip.button->setChecked(chip.status == m_status_filter);
	}
}

void LibraryPage::UpdateHero() {
	if (m_hero == nullptr) {
		return;
	}
	const bool show = !m_list_mode && m_mode != Mode::Compatibility && !m_visible_items.isEmpty();
	m_hero->setVisible(show);
	if (!show) {
		return;
	}

	auto* item = HeroItem();
	if (item == nullptr) {
		m_hero->setVisible(false);
		return;
	}
	const auto cover = m_covers->Get(item->GetInfo().basedir);
	m_hero->SetItem(item, cover, LastPlayedText(item), m_run_enabled);
}

void LibraryPage::UpdateDetails() {
	if (m_details_stack == nullptr) {
		return;
	}
	if (m_selected == nullptr) {
		m_details_stack->setCurrentIndex(1);
		return;
	}
	m_details_stack->setCurrentIndex(0);

	const auto& info = m_selected->GetInfo();
	m_detail_cover->SetSeed(info.name);
	m_detail_cover->SetImage(m_covers->Get(info.basedir));
	m_detail_title->SetFullText(info.name);

	QStringList meta;
	if (!info.title_id.isEmpty()) {
		meta.append(info.title_id);
	}
	if (!info.gameVersion.isEmpty()) {
		meta.append(QStringLiteral("应用版本 %1").arg(info.gameVersion));
	}
	m_detail_meta->setText(meta.join(QStringLiteral(" · ")));

	m_detail_status->SetStatus(info.game_status);
	m_detail_community->setText(info.game_comment.isEmpty()
	                                ? QStringLiteral("社区兼容性数据")
	                                : QStringLiteral("社区数据"));
	m_detail_compat_edit->setVisible(m_controller->CanEditCompatibility(*m_selected));
	m_detail_firmware->setText(info.firmwareVer.isEmpty() ? QStringLiteral("\u2014")
	                                                      : info.firmwareVer);
	const auto size = m_selected->text(GAME_SIZE_COLUMN);
	m_detail_size->setText(size.isEmpty() ? QStringLiteral("\u2014") : size);

	const auto path = !info.game_path.isEmpty() ? info.game_path : info.basedir;
	const auto path_font = m_detail_path->font();
	m_detail_path->setText(LauncherUi::ElideText(path_font, path, 185));
	m_detail_path->setToolTip(path);

	const bool is_last = NormalizePath(info.game_path.isEmpty() ? info.basedir : info.game_path) ==
	                     NormalizePath(m_last_played_path);
	m_detail_last_run->setText(is_last && m_last_played_time.isValid()
	                               ? LauncherUi::RelativeTime(m_last_played_time)
	                               : QStringLiteral("\u2014"));

	const bool running = m_selected->IsRunning();
	m_detail_play->setText(running ? QStringLiteral("运行中") : QStringLiteral("开始游戏"));
	m_detail_play->setEnabled(m_run_enabled && !running);
	m_detail_settings->setEnabled(!running);
	m_detail_input->setEnabled(!running);

	const bool favorite = m_favorites.contains(FavoriteKey(m_selected));
	{
		const QSignalBlocker blocker(m_detail_favorite);
		m_detail_favorite->setChecked(favorite);
	}
	m_detail_favorite->setIcon(LauncherUi::TintedIcon(
	    favorite ? QStringLiteral(":/icons/star-fill.svg") : QStringLiteral(":/icons/star.svg"),
	    favorite ? QColor(QStringLiteral("#F2B33D")) : QColor(QStringLiteral("#9BA4B8")),
	    QSize(15, 15)));
	m_detail_favorite->setToolTip(favorite ? QStringLiteral("取消收藏") : QStringLiteral("收藏"));

	const bool has_trophy = TrophyViewerDialog::HasTrophyData(&info);
	m_detail_trophy_value->setText(has_trophy ? QStringLiteral("本地数据") : QStringLiteral("\u2014"));
	m_detail_trophy_note->setText(has_trophy ? QStringLiteral("已找到本地奖杯数据")
	                                         : QStringLiteral("运行游戏后可同步奖杯数据"));
	m_detail_trophy->setEnabled(has_trophy);
	m_detail_trophy->setVisible(has_trophy);

	m_detail_note->setText(
	    info.game_comment.isEmpty()
	        ? QStringLiteral("兼容性数据由社区维护，最后同步于本地数据库。")
	        : info.game_comment);
}

void LibraryPage::UpdateEmptyState() {
	const bool empty = m_visible_items.isEmpty();
	if (empty) {
		m_content->setCurrentWidget(m_empty_page);
	} else {
		m_content->setCurrentWidget(m_list_mode ? m_list_page : m_grid_page);
	}

	if (!empty) {
		return;
	}

	QString icon;
	QString title;
	QString text;
	QString button;
	switch (m_mode) {
		case Mode::Library:
			icon   = QStringLiteral(":/icons/folder-plus.svg");
			title  = QStringLiteral("还没有添加游戏目录");
			text   = QStringLiteral("添加包含 PS5 游戏的文件夹，启动器会自动扫描 eboot.bin 与 ZArchive。");
			button = QStringLiteral("添加游戏目录");
			break;
		case Mode::Favorites:
			icon   = QStringLiteral(":/icons/star.svg");
			title  = QStringLiteral("还没有收藏的游戏");
			text   = QStringLiteral("在游戏详情中点击星标，或在卡片上右键即可收藏。");
			button = QStringLiteral("去游戏库看看");
			break;
		case Mode::Compatibility:
			icon   = QStringLiteral(":/icons/shield.svg");
			title  = QStringLiteral("还没有兼容性数据");
			text   = QStringLiteral("添加游戏目录后，启动器会同步社区兼容性状态。");
			button = QStringLiteral("前往游戏库");
			break;
	}
	m_empty_icon->setPixmap(LauncherUi::TintedIcon(icon, QColor(QStringLiteral("#3D8BFF")),
	                                               QSize(40, 40))
	                            .pixmap(40, 40));
	m_empty_title->setText(title);
	m_empty_text->setText(text);
	m_empty_button->setText(button);
	m_empty_button->setIcon(LauncherUi::TintedIcon(
	    m_mode == Mode::Library ? QStringLiteral(":/icons/plus.svg")
	                            : QStringLiteral(":/icons/arrow-right.svg"),
	    QColor(Qt::white), QSize(11, 11)));
	m_empty_button->setIconSize(QSize(11, 11));
}

void LibraryPage::UpdateViewButtons() {
	if (m_grid_button == nullptr) {
		return;
	}
	const QSignalBlocker grid_blocker(m_grid_button);
	const QSignalBlocker list_blocker(m_list_button);
	m_grid_button->setChecked(!m_list_mode);
	m_list_button->setChecked(m_list_mode);
	m_sort_button->setVisible(m_list_mode);
}

void LibraryPage::UpdateSortButton() {
	if (m_sort_button == nullptr) {
		return;
	}

	QString key;
	switch (m_sort_key) {
		case SortKey::Name: key = QStringLiteral("按名称"); break;
		case SortKey::Serial: key = QStringLiteral("按 Serial"); break;
		case SortKey::Size: key = QStringLiteral("按大小"); break;
		case SortKey::Status: key = QStringLiteral("按状态"); break;
		case SortKey::GameVersion: key = QStringLiteral("按游戏版本"); break;
		case SortKey::FirmwareVersion: key = QStringLiteral("按固件版本"); break;
		case SortKey::Path: key = QStringLiteral("按路径"); break;
		case SortKey::Comment: key = QStringLiteral("按备注"); break;
	}
	m_sort_button->setText(
	    QStringLiteral("%1 · %2").arg(key, m_sort_ascending ? QStringLiteral("升序")
	                                                         : QStringLiteral("降序")));
}

void LibraryPage::ShowSortMenu() {
	if (m_sort_button == nullptr) {
		return;
	}

	QMenu menu(this);
	const auto add_key = [this, &menu](const QString& text, SortKey key) {
		auto* action = menu.addAction(text);
		action->setCheckable(true);
		action->setChecked(m_sort_key == key);
		connect(action, &QAction::triggered, this, [this, key]() {
			m_sort_key = key;
			SortItems(m_items);
			UpdateSortButton();
			RebuildViews();
		});
	};
	add_key(QStringLiteral("按名称"), SortKey::Name);
	add_key(QStringLiteral("按 Serial"), SortKey::Serial);
	add_key(QStringLiteral("按大小"), SortKey::Size);
	add_key(QStringLiteral("按状态"), SortKey::Status);
	add_key(QStringLiteral("按游戏版本"), SortKey::GameVersion);
	add_key(QStringLiteral("按固件版本"), SortKey::FirmwareVersion);
	add_key(QStringLiteral("按路径"), SortKey::Path);
	add_key(QStringLiteral("按备注"), SortKey::Comment);

	menu.addSeparator();
	const auto add_order = [this, &menu](const QString& text, bool ascending) {
		auto* action = menu.addAction(text);
		action->setCheckable(true);
		action->setChecked(m_sort_ascending == ascending);
		connect(action, &QAction::triggered, this, [this, ascending]() {
			m_sort_ascending = ascending;
			SortItems(m_items);
			UpdateSortButton();
			RebuildViews();
		});
	};
	add_order(QStringLiteral("升序"), true);
	add_order(QStringLiteral("降序"), false);

	menu.exec(m_sort_button->mapToGlobal(
	    QPoint(0, m_sort_button->height() + 4)));
}

void LibraryPage::SortItems(QVector<ConfigurationItem*>& items) {
	std::stable_sort(items.begin(), items.end(),
	                 [this](ConfigurationItem* left, ConfigurationItem* right) {
		                 int result = 0;
		                 switch (m_sort_key) {
			                 case SortKey::Name:
				                 result = QString::compare(left->GetInfo().name,
				                                           right->GetInfo().name,
				                                           Qt::CaseInsensitive);
				                 break;
			                 case SortKey::Serial:
				                 result = QString::compare(left->GetInfo().title_id,
				                                           right->GetInfo().title_id,
				                                           Qt::CaseInsensitive);
				                 break;
			                 case SortKey::Size: {
				                 const auto left_size =
				                     left->data(GAME_SIZE_COLUMN, Qt::UserRole).toLongLong();
				                 const auto right_size =
				                     right->data(GAME_SIZE_COLUMN, Qt::UserRole).toLongLong();
				                 result = left_size < right_size
				                              ? -1
				                              : (left_size > right_size ? 1 : 0);
				                 break;
			                 }
			                 case SortKey::Status:
				                 result = static_cast<int>(
				                              left->GetInfo().game_status) -
				                          static_cast<int>(right->GetInfo().game_status);
				                 break;
			                 case SortKey::GameVersion:
				                 result = QVersionNumber::compare(
				                     QVersionNumber::fromString(left->GetInfo().gameVersion),
				                     QVersionNumber::fromString(right->GetInfo().gameVersion));
				                 break;
			                 case SortKey::FirmwareVersion:
				                 result = QVersionNumber::compare(
				                     QVersionNumber::fromString(left->GetInfo().firmwareVer),
				                     QVersionNumber::fromString(right->GetInfo().firmwareVer));
				                 break;
			                 case SortKey::Path:
				                 result = QString::compare(
				                     left->GetInfo().game_path.isEmpty()
				                         ? left->GetInfo().basedir
				                         : left->GetInfo().game_path,
				                     right->GetInfo().game_path.isEmpty()
				                         ? right->GetInfo().basedir
				                         : right->GetInfo().game_path,
				                     Qt::CaseInsensitive);
				                 break;
			                 case SortKey::Comment:
				                 result = QString::compare(left->GetInfo().game_comment,
				                                           right->GetInfo().game_comment,
				                                           Qt::CaseInsensitive);
				                 break;
		                 }
		                 if (result == 0) {
			                 result = QString::compare(left->GetInfo().name,
			                                           right->GetInfo().name,
			                                           Qt::CaseInsensitive);
		                 }
		                 return m_sort_ascending ? result < 0 : result > 0;
	                 });
}

QVector<ConfigurationItem*> LibraryPage::BaseItems() const {
	QVector<ConfigurationItem*> items;
	items.reserve(m_items.size());
	for (auto* item: m_items) {
		if (MatchesMode(item) && MatchesDirectory(item)) {
			items.append(item);
		}
	}
	return items;
}

QVector<ConfigurationItem*> LibraryPage::VisibleItems() const {
	QVector<ConfigurationItem*> items;
	items.reserve(m_items.size());
	for (auto* item: m_items) {
		if (Matches(item)) {
			items.append(item);
		}
	}
	return items;
}

bool LibraryPage::MatchesMode(ConfigurationItem* item) const {
	return m_mode != Mode::Favorites || m_favorites.contains(FavoriteKey(item));
}

bool LibraryPage::MatchesDirectory(ConfigurationItem* item) const {
	if (m_directory_filter.isEmpty()) {
		return true;
	}
	const auto& info = item->GetInfo();
	return PathUnder(info.basedir, m_directory_filter) ||
	       PathUnder(info.game_path, m_directory_filter);
}

bool LibraryPage::Matches(ConfigurationItem* item) const {
	if (!MatchesMode(item) || !MatchesDirectory(item)) {
		return false;
	}
	if (m_status_filter >= 0 &&
	    static_cast<int>(item->GetInfo().game_status) != m_status_filter) {
		return false;
	}
	const auto query = m_search->text().trimmed();
	if (query.isEmpty()) {
		return true;
	}
	const auto& info = item->GetInfo();
	return info.name.contains(query, Qt::CaseInsensitive) ||
	       info.title_id.contains(query, Qt::CaseInsensitive);
}

QString LibraryPage::FavoriteKey(ConfigurationItem* item) const {
	const auto& info = item->GetInfo();
	return NormalizePath(info.game_path.isEmpty() ? info.basedir : info.game_path);
}

ConfigurationItem* LibraryPage::FindLastPlayed() const {
	if (m_last_played_path.isEmpty()) {
		return nullptr;
	}
	for (auto* item: m_items) {
		const auto& info = item->GetInfo();
		if (NormalizePath(info.game_path.isEmpty() ? info.basedir : info.game_path) ==
		        NormalizePath(m_last_played_path) &&
		    MatchesMode(item)) {
			return item;
		}
	}
	return nullptr;
}

ConfigurationItem* LibraryPage::HeroItem() const {
	if (auto* last = FindLastPlayed(); last != nullptr) {
		return last;
	}
	if (m_selected != nullptr && MatchesMode(m_selected)) {
		return m_selected;
	}
	for (auto* item: m_visible_items) {
		if (MatchesMode(item)) {
			return item;
		}
	}
	return nullptr;
}

QString LibraryPage::LastPlayedText(ConfigurationItem* item) const {
	if (item == nullptr || !m_last_played_time.isValid()) {
		return {};
	}
	const auto& info = item->GetInfo();
	if (NormalizePath(info.game_path.isEmpty() ? info.basedir : info.game_path) !=
	    NormalizePath(m_last_played_path)) {
		return {};
	}
	return LauncherUi::RelativeTime(m_last_played_time);
}

int LibraryPage::RowForItem(ConfigurationItem* item) const {
	if (item == nullptr) {
		return -1;
	}
	for (int row = 0; row < m_table->rowCount(); row++) {
		if (ItemForRow(row) == item) {
			return row;
		}
	}
	return -1;
}

ConfigurationItem* LibraryPage::ItemForRow(int row) const {
	if (row < 0 || row >= m_table->rowCount()) {
		return nullptr;
	}
	auto* item = m_table->item(row, 0);
	if (item == nullptr) {
		return nullptr;
	}
	return reinterpret_cast<ConfigurationItem*>(
	    static_cast<quintptr>(item->data(Qt::UserRole).toULongLong()));
}

void LibraryPage::SelectItem(ConfigurationItem* item, bool scroll_table) {
	if (item == nullptr) {
		return;
	}
	m_controller->SelectItemByPointer(item);
	if (scroll_table && m_table != nullptr) {
		const int row = RowForItem(item);
		if (row >= 0) {
			const QSignalBlocker blocker(m_table);
			m_syncing = true;
			m_table->setCurrentCell(row, 0);
			m_table->scrollToItem(m_table->item(row, 0));
			m_syncing = false;
		}
	}
}

void LibraryPage::PlayItem(ConfigurationItem* item) {
	if (item == nullptr) {
		return;
	}
	SelectItem(item);
	m_controller->RequestRun();
}

void LibraryPage::ToggleFavorite(ConfigurationItem* item) {
	if (item == nullptr) {
		return;
	}
	const auto key = FavoriteKey(item);
	if (m_favorites.contains(key)) {
		m_favorites.remove(key);
	} else {
		m_favorites.insert(key);
	}

	for (auto* card: m_cards) {
		card->SetFavorite(m_favorites.contains(FavoriteKey(card->GetItem())));
	}
	UpdateDetails();
	emit FavoritesChanged(m_favorites);

	if (m_mode == Mode::Favorites) {
		RebuildViews();
	}
}

void LibraryPage::ShowCompatibilityEditor() {
	if (m_selected == nullptr || !m_controller->CanEditCompatibility(*m_selected)) {
		return;
	}

	const auto title_id        = m_selected->GetInfo().title_id;
	const auto current_status  = m_selected->GetInfo().game_status;
	const auto current_comment = m_selected->GetInfo().game_comment;

	QDialog dialog(this);
	dialog.setWindowTitle(QStringLiteral("编辑兼容性"));
	auto* layout = new QVBoxLayout(&dialog);
	auto* form   = new QFormLayout;

	auto* status = new QComboBox(&dialog);
	status->setObjectName(QStringLiteral("SettingsCombo"));
	const Configuration::GameStatus statuses[] = {
	    Configuration::GameStatus::Unknown, Configuration::GameStatus::MainMenu,
	    Configuration::GameStatus::InGame, Configuration::GameStatus::Logo,
	    Configuration::GameStatus::DoesntBoot};
	for (const auto value: statuses) {
		status->addItem(LauncherUi::StatusText(value), static_cast<int>(value));
	}
	const int current = status->findData(static_cast<int>(current_status));
	status->setCurrentIndex(current >= 0 ? current : 0);

	auto* comment = new QLineEdit(&dialog);
	comment->setText(current_comment);
	comment->setPlaceholderText(QStringLiteral("兼容性备注"));

	form->addRow(QStringLiteral("状态"), status);
	form->addRow(QStringLiteral("备注"), comment);
	layout->addLayout(form);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);

	if (dialog.exec() == QDialog::Accepted) {
		m_controller->SetCompatibilityComment(title_id, comment->text());
		m_controller->SetCompatibilityStatus(
		    title_id,
		    static_cast<Configuration::GameStatus>(status->currentData().toInt()));
	}
}

void LibraryPage::ShowGameMenu(ConfigurationItem* item, const QPoint& global_pos) {
	if (item == nullptr) {
		return;
	}
	SelectItem(item);

	const auto& info   = item->GetInfo();
	const bool  running = item->IsRunning();
	const bool  favorite = m_favorites.contains(FavoriteKey(item));
	const auto  base     = info.basedir;

	QMenu menu(this);
	auto* run = menu.addAction(QStringLiteral("开始游戏"));
	run->setEnabled(m_run_enabled && !running);
	connect(run, &QAction::triggered, this, [this, item]() { PlayItem(item); });

	auto* details = menu.addAction(QStringLiteral("查看详情"));
	connect(details, &QAction::triggered, this, [this, item]() {
		SetListMode(true);
		SelectItem(item, true);
	});

	auto* view_trophies = menu.addAction(QStringLiteral("查看奖杯"));
	view_trophies->setEnabled(TrophyViewerDialog::HasTrophyData(&info));
	connect(view_trophies, &QAction::triggered, this,
	        [this]() { m_controller->ViewTrophies(); });

	menu.addSeparator();
	auto* favorite_action =
	    menu.addAction(favorite ? QStringLiteral("取消收藏") : QStringLiteral("收藏"));
	connect(favorite_action, &QAction::triggered, this, [this, item]() { ToggleFavorite(item); });

	auto* open_folder = menu.addAction(QStringLiteral("打开游戏目录"));
	open_folder->setEnabled(QDir(base).exists() || GameContent::IsArchive(base));
	connect(open_folder, &QAction::triggered, this,
	        [this]() { m_controller->OpenSelectedGameFolder(); });

	auto* settings = menu.addAction(QStringLiteral("游戏设置…"));
	settings->setEnabled(!running);
	connect(settings, &QAction::triggered, this, [this]() { m_controller->EditSelectedGame(); });

	auto* input = menu.addAction(QStringLiteral("输入映射…"));
	input->setEnabled(!running);
	connect(input, &QAction::triggered, this,
	        [this]() { m_controller->EditInputMapping(); });

	auto* addcont = menu.addAction(QStringLiteral("附加内容…"));
	connect(addcont, &QAction::triggered, this,
	        [this]() { m_controller->ShowAdditionalContentForSelected(); });

	auto* patches = menu.addAction(QStringLiteral("管理补丁…"));
	patches->setEnabled(!running);
	connect(patches, &QAction::triggered, this,
	        [this]() { m_controller->ShowPatchesForSelected(); });

	menu.addSeparator();
	auto* compress = menu.addAction(QStringLiteral("压缩为 ZAR…"));
	compress->setEnabled(!running && QDir(base).exists());
	connect(compress, &QAction::triggered, this,
	        [this]() { m_controller->CompressSelectedGame(); });

	auto* saves = menu.addAction(QStringLiteral("打开存档目录"));
	saves->setEnabled(!running);
	connect(saves, &QAction::triggered, this,
	        [this]() { m_controller->OpenSelectedSaveDataFolder(); });

	auto* remove_saves = menu.addAction(QStringLiteral("删除存档…"));
	remove_saves->setEnabled(!running);
	connect(remove_saves, &QAction::triggered, this,
	        [this]() { m_controller->RemoveSelectedSaveData(); });

	menu.addSeparator();
	auto* clear = menu.addAction(QStringLiteral("清除游戏配置"));
	clear->setEnabled(!running && info.custom_settings);
	connect(clear, &QAction::triggered, this,
	        [this]() { m_controller->DeleteSelectedGame(); });

	menu.exec(global_pos);
}

void LibraryPage::ShowHeaderMenu() {
	QMenu menu(this);
	auto* rescan = menu.addAction(QStringLiteral("重新扫描游戏"));
	connect(rescan, &QAction::triggered, this, &LibraryPage::Rescan);

	auto* add = menu.addAction(QStringLiteral("添加游戏目录…"));
	connect(add, &QAction::triggered, this, &LibraryPage::AddGameDirectory);

	auto* trophies = menu.addAction(QStringLiteral("奖杯总览…"));
	connect(trophies, &QAction::triggered, this,
	        [this]() { m_controller->ViewTrophyOverview(); });

	menu.addSeparator();
	auto* global = menu.addAction(QStringLiteral("全局设置…"));
	connect(global, &QAction::triggered, this,
	        [this]() { m_controller->EditGlobalSettings(); });

	auto* input = menu.addAction(QStringLiteral("输入映射…"));
	connect(input, &QAction::triggered, this,
	        [this]() { m_controller->EditInputMapping(); });

	menu.addSeparator();
	auto* settings_folder = menu.addAction(QStringLiteral("打开设置文件位置"));
	connect(settings_folder, &QAction::triggered, this,
	        [this]() { m_controller->OpenSettingsFileFolder(); });

	menu.exec(m_more_button->mapToGlobal(QPoint(0, m_more_button->height() + 4)));
}

void LibraryPage::Rescan() {
	m_controller->ScanGameDirectory();
}

void LibraryPage::AddGameDirectory() {
	m_controller->AddGameDirectoryInteractive(this);
}

void LibraryPage::ScheduleCoverLoads() {
	for (int index = 0; index < m_cards.size(); index++) {
		auto* card = m_cards.at(index);
		QTimer::singleShot(index * 20, card, [card]() { card->LoadCover(); });
	}
}

void LibraryPage::OnCardClicked(GameCard* card) {
	if (card != nullptr) {
		SelectItem(card->GetItem());
	}
}

void LibraryPage::OnCardDoubleClicked(GameCard* card) {
	if (card != nullptr) {
		PlayItem(card->GetItem());
	}
}

void LibraryPage::OnCardContextMenu(GameCard* card, const QPoint& global_pos) {
	if (card != nullptr) {
		ShowGameMenu(card->GetItem(), global_pos);
	}
}

void LibraryPage::OnTableCurrentChanged(int row, int /*column*/) {
	if (m_syncing) {
		return;
	}
	if (auto* item = ItemForRow(row); item != nullptr) {
		SelectItem(item);
	}
}
