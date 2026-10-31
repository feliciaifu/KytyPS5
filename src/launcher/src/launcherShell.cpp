#include "launcherShell.h"

#include "cmake_config.h"
#include "configurationItem.h"
#include "configurationListWidget.h"
#include "coverLoader.h"
#include "gameCards.h"
#include "gameViews.h"
#include "launcherUiUtils.h"
#include "libraryPage.h"
#include "settingsPage.h"

#include <QAction>
#include <QButtonGroup>
#include <QKeySequence>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#if defined(_WIN32)
#include <windows.h> // IWYU pragma: keep
#endif

#include <functional>

namespace {

constexpr int kSidebarWidth = 236;

void ToggleMaximizeWindow(QWidget* window) {
#if defined(_WIN32)
	auto hwnd = reinterpret_cast<HWND>(window->winId());
	ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
#else
	if (window->isMaximized()) {
		window->showNormal();
	} else {
		window->showMaximized();
	}
#endif
}

class TitleBarWidget: public QWidget {
public:
	explicit TitleBarWidget(QWidget* window, QWidget* parent = nullptr)
	    : QWidget(parent), m_window(window) {}

protected:
	void mousePressEvent(QMouseEvent* event) override {
		if (event->button() == Qt::LeftButton) {
			if (auto* handle = m_window->windowHandle(); handle != nullptr) {
				handle->startSystemMove();
			}
			event->accept();
			return;
		}
		QWidget::mousePressEvent(event);
	}

	void mouseDoubleClickEvent(QMouseEvent* event) override {
		if (event->button() == Qt::LeftButton) {
			if (m_window->isMaximized()) {
				m_window->showNormal();
			} else {
				m_window->showMaximized();
			}
			event->accept();
			return;
		}
		QWidget::mouseDoubleClickEvent(event);
	}

private:
	QWidget* m_window = nullptr;
};

QString NormalizePath(const QString& path) {
	return QDir::cleanPath(path.trimmed());
}

QPushButton* MakeNavButton(const QString& text, const QString& icon, QWidget* parent) {
	auto* button = new QPushButton(parent);
	button->setObjectName(QStringLiteral("SidebarItem"));
	button->setCheckable(true);
	button->setFixedHeight(34);
	button->setCursor(Qt::PointingHandCursor);
	button->setIcon(LauncherUi::TintedIcon(icon, QColor(QStringLiteral("#9BA4B8")), QSize(18, 18)));
	button->setIconSize(QSize(18, 18));
	button->setText(text);
	return button;
}

} // namespace

class SavesPage: public QWidget {
public:
	SavesPage(ConfigurationListWidget* controller, CoverLoader* covers,
	          std::function<void()> go_library, QWidget* parent = nullptr)
	    : QWidget(parent), m_controller(controller), m_covers(covers),
	      m_go_library(std::move(go_library)) {
		auto* root = new QVBoxLayout(this);
		root->setContentsMargins(32, 24, 32, 24);
		root->setSpacing(16);

		auto* title = new QLabel(QStringLiteral("存档与补丁"), this);
		title->setObjectName(QStringLiteral("PageTitle"));
		auto* subtitle = new QLabel(QStringLiteral("管理所选游戏的存档、补丁与附加内容。"), this);
		subtitle->setObjectName(QStringLiteral("PageSubtitle"));
		root->addWidget(title);
		root->addWidget(subtitle);

		auto* card = new QFrame(this);
		card->setObjectName(QStringLiteral("SettingsCard"));
		auto* card_layout = new QVBoxLayout(card);
		card_layout->setContentsMargins(18, 18, 18, 18);
		card_layout->setSpacing(14);

		auto* summary = new QHBoxLayout;
		summary->setContentsMargins(0, 0, 0, 0);
		summary->setSpacing(16);
		m_cover = new CoverView(card);
		m_cover->setFixedSize(128, 72);
		m_cover->SetCornerRadius(8);
		summary->addWidget(m_cover);

		auto* info = new QVBoxLayout;
		info->setContentsMargins(0, 0, 0, 0);
		info->setSpacing(5);
		m_title = new ElidedLabel(card);
		m_title->setObjectName(QStringLiteral("DetailTitle"));
		m_meta = new QLabel(card);
		m_meta->setObjectName(QStringLiteral("DetailMeta"));
		auto* status_row = new QHBoxLayout;
		status_row->setContentsMargins(0, 0, 0, 0);
		status_row->setSpacing(8);
		m_status = new StatusPill(card);
		status_row->addWidget(m_status);
		status_row->addStretch(1);
		info->addWidget(m_title);
		info->addWidget(m_meta);
		info->addLayout(status_row);
		summary->addLayout(info, 1);
		card_layout->addLayout(summary);

		auto* separator = new QFrame(card);
		separator->setFrameShape(QFrame::HLine);
		card_layout->addWidget(separator);

		auto* actions = new QGridLayout;
		actions->setContentsMargins(0, 0, 0, 0);
		actions->setHorizontalSpacing(10);
		actions->setVerticalSpacing(10);
		int row = 0;
		int column = 0;
		const auto add_action = [this, actions, &row, &column, card](const QString& text,
		                                                            auto&& handler) {
			auto* button = new QPushButton(text, card);
			button->setObjectName(QStringLiteral("SecondaryButton"));
			button->setFixedHeight(34);
			button->setCursor(Qt::PointingHandCursor);
			connect(button, &QPushButton::clicked, this, handler);
			actions->addWidget(button, row, column);
			m_actions.append(button);
			column++;
			if (column == 2) {
				column = 0;
				row++;
			}
		};
		add_action(QStringLiteral("打开游戏目录"),
		           [this]() { m_controller->OpenSelectedGameFolder(); });
		add_action(QStringLiteral("打开存档目录"),
		           [this]() { m_controller->OpenSelectedSaveDataFolder(); });
		add_action(QStringLiteral("管理补丁"),
		           [this]() { m_controller->ShowPatchesForSelected(); });
		add_action(QStringLiteral("附加内容"),
		           [this]() { m_controller->ShowAdditionalContentForSelected(); });
		add_action(QStringLiteral("压缩为 ZAR"),
		           [this]() { m_controller->CompressSelectedGame(); });
		add_action(QStringLiteral("删除存档"),
		           [this]() { m_controller->RemoveSelectedSaveData(); });
		card_layout->addLayout(actions);

		m_empty = new QWidget(card);
		m_empty->setObjectName(QStringLiteral("Transparent"));
		auto* empty_layout = new QHBoxLayout(m_empty);
		empty_layout->setContentsMargins(0, 8, 0, 0);
		auto* empty_label =
		    new QLabel(QStringLiteral("还没有选择游戏。先在游戏库中选择一个游戏。"), m_empty);
		empty_label->setObjectName(QStringLiteral("DetailNote"));
		empty_layout->addWidget(empty_label, 1);
		auto* go = new QPushButton(QStringLiteral("前往游戏库"), m_empty);
		go->setObjectName(QStringLiteral("PrimaryButton"));
		go->setFixedHeight(34);
		go->setCursor(Qt::PointingHandCursor);
		connect(go, &QPushButton::clicked, this, [this]() {
			if (m_go_library) {
				m_go_library();
			}
		});
		empty_layout->addWidget(go);
		card_layout->addWidget(m_empty);

		root->addWidget(card);
		root->addStretch(1);

		connect(controller, &ConfigurationListWidget::SelectionChanged, this,
		        [this]() { Refresh(); });
		connect(controller, &ConfigurationListWidget::ItemsChanged, this,
		        [this]() { Refresh(); });
		Refresh();
	}

	void Refresh() {
		auto* item = m_controller->GetSelectedItem();
		const bool has_item = item != nullptr;
		m_empty->setVisible(!has_item);
		for (auto* button: m_actions) {
			button->setEnabled(has_item && !item->IsRunning());
		}

		if (!has_item) {
			m_cover->SetSeed({});
			m_cover->SetImage({});
			m_title->SetFullText(QStringLiteral("未选择游戏"));
			m_meta->setText({});
			m_status->SetStatus(Configuration::GameStatus::Unknown);
			return;
		}

		const auto& info = item->GetInfo();
		m_cover->SetSeed(info.name);
		m_cover->SetImage(m_covers->Get(info.basedir));
		m_title->SetFullText(info.name);

		QStringList meta;
		if (!info.title_id.isEmpty()) {
			meta.append(info.title_id);
		}
		if (!info.gameVersion.isEmpty()) {
			meta.append(QStringLiteral("应用版本 %1").arg(info.gameVersion));
		}
		const auto path = !info.game_path.isEmpty() ? info.game_path : info.basedir;
		if (!path.isEmpty()) {
			meta.append(path);
		}
		m_meta->setText(meta.join(QStringLiteral(" · ")));
		m_status->SetStatus(info.game_status);
	}

private:
	ConfigurationListWidget* m_controller = nullptr;
	CoverLoader*             m_covers = nullptr;
	std::function<void()>    m_go_library;

	CoverView*  m_cover = nullptr;
	ElidedLabel* m_title = nullptr;
	QLabel*     m_meta = nullptr;
	StatusPill* m_status = nullptr;
	QWidget*    m_empty = nullptr;
	QVector<QPushButton*> m_actions;
};

LauncherShell::LauncherShell(ConfigurationListWidget* controller, QWidget* parent)
    : QWidget(parent), m_controller(controller) {
	setObjectName(QStringLiteral("LauncherRoot"));
	m_covers = new CoverLoader;

	BuildUi();

	const auto vulkan = LauncherUi::QueryVulkanInfo();
	if (vulkan.valid) {
		m_runtime_api    = vulkan.version;
		m_runtime_device = vulkan.device;
	}
	m_settings->SetRuntimeInfo(m_runtime_device, m_runtime_api);

	LoadUiSettings();

	connect(m_controller, &ConfigurationListWidget::ItemsChanged, this,
	        [this]() { RefreshSidebar(); });
	connect(m_controller, &ConfigurationListWidget::Run, this, [this]() { RecordLastPlayed(); });

	connect(m_library, &LibraryPage::FavoritesChanged, this,
	        [this](const QSet<QString>& favorites) {
		        m_favorites = favorites;
		        m_library->SetFavorites(m_favorites);
		        SaveUiSettings();
	        });
	connect(m_library, &LibraryPage::RequestLibraryMode, this,
	        [this]() { NavigateTo(Section::Library); });
	connect(m_library, &LibraryPage::ViewModeChanged, this, [this](bool) { SaveUiSettings(); });

	connect(m_settings, &SettingsPage::CheckForUpdatesRequested, this,
	        &LauncherShell::CheckForUpdatesRequested);
	connect(m_settings, &SettingsPage::CheckOnStartupChanged, this,
	        &LauncherShell::CheckOnStartupChanged);
	connect(m_settings, &SettingsPage::GridColumnsChanged, this, [this](int columns) {
		m_library->SetGridColumns(columns);
		SaveUiSettings();
	});
	connect(m_settings, &SettingsPage::ResetRequested, this, [this]() { ResetUi(); });

	m_ui_loaded = true;

	auto* refresh_action = new QAction(this);
	refresh_action->setShortcut(QKeySequence::Refresh);
	refresh_action->setShortcutContext(Qt::WindowShortcut);
	addAction(refresh_action);
	connect(refresh_action, &QAction::triggered, this,
	        [this]() { m_controller->ScanGameDirectory(); });

	SetVersion(QStringLiteral(KYTY_VERSION));
	UpdateSidebarIcons();
	RefreshSidebar();
	UpdateRuntimeCard();
	NavigateTo(Section::Library);
}

void LauncherShell::BuildUi() {
	auto* root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);

	m_title_bar = BuildTitleBar();
	root->addWidget(m_title_bar);

	auto* body = new QHBoxLayout;
	body->setContentsMargins(0, 0, 0, 0);
	body->setSpacing(0);
	body->addWidget(BuildSidebar());

	m_pages = new QStackedWidget(this);
	m_pages->setObjectName(QStringLiteral("LauncherContent"));
	m_pages->addWidget(m_library = new LibraryPage(m_controller, m_covers, m_pages));
	m_pages->addWidget(
	    m_saves = new SavesPage(m_controller, m_covers, [this]() { NavigateTo(Section::Library); },
	                            m_pages));
	m_pages->addWidget(m_settings = new SettingsPage(m_controller, m_pages));
	body->addWidget(m_pages, 1);
	root->addLayout(body, 1);
}

QWidget* LauncherShell::BuildTitleBar() {
	auto* bar = new TitleBarWidget(window(), this);
	bar->setObjectName(QStringLiteral("TitleBar"));
	bar->setFixedHeight(40);

	auto* layout = new QHBoxLayout(bar);
	layout->setContentsMargins(18, 0, 8, 0);
	layout->setSpacing(10);

	auto* logo = new QLabel(QStringLiteral("K"), bar);
	logo->setObjectName(QStringLiteral("LogoMark"));
	logo->setFixedSize(22, 22);
	logo->setAlignment(Qt::AlignCenter);
	logo->setAttribute(Qt::WA_TransparentForMouseEvents);
	layout->addWidget(logo);

	auto* name = new QLabel(QStringLiteral("KytyPS5"), bar);
	name->setObjectName(QStringLiteral("TitleText"));
	name->setAttribute(Qt::WA_TransparentForMouseEvents);
	layout->addWidget(name);

	m_version_chip = new QLabel(bar);
	m_version_chip->setObjectName(QStringLiteral("VersionChip"));
	m_version_chip->setAttribute(Qt::WA_TransparentForMouseEvents);
	m_version_chip->setAlignment(Qt::AlignCenter);
	m_version_chip->setFixedHeight(18);
	m_version_chip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	layout->addWidget(m_version_chip, 0, Qt::AlignVCenter);
	layout->addStretch(1);

	const auto add_window_button = [this, bar, layout](const QString& text,
	                                                   const QString& object_name,
	                                                   std::function<void()> handler) {
		auto* button = new QPushButton(text, bar);
		button->setObjectName(object_name);
		button->setFixedSize(32, 24);
		button->setFlat(true);
		button->setCursor(Qt::PointingHandCursor);
		connect(button, &QPushButton::clicked, this, std::move(handler));
		layout->addWidget(button, 0, Qt::AlignVCenter);
		return button;
	};

	auto* minimize = add_window_button(QStringLiteral("\u2014"), QStringLiteral("WindowButton"),
	                                   [this]() { window()->showMinimized(); });
	Q_UNUSED(minimize);
	m_maximize_button =
	    add_window_button(QStringLiteral("\u25A1"), QStringLiteral("WindowButton"),
	                      [this]() { ToggleMaximizeWindow(window()); });
	auto* close = add_window_button(QStringLiteral("\u2715"), QStringLiteral("WindowCloseButton"),
	                                [this]() { window()->close(); });
	Q_UNUSED(close);

	return bar;
}

QWidget* LauncherShell::BuildSidebar() {
	auto* sidebar = new QWidget(this);
	sidebar->setObjectName(QStringLiteral("Sidebar"));
	sidebar->setFixedWidth(kSidebarWidth);

	auto* layout = new QVBoxLayout(sidebar);
	layout->setContentsMargins(12, 16, 12, 12);
	layout->setSpacing(2);

	auto* group = new QButtonGroup(sidebar);
	group->setExclusive(true);

	const auto add_section = [layout](const QString& text) {
		auto* label = new QLabel(text);
		label->setObjectName(QStringLiteral("NavSection"));
		layout->addWidget(label);
	};

	const auto add_nav = [this, layout, group](const QString& text, const QString& icon,
	                                           QPushButton** button) {
		auto* item = MakeNavButton(text, icon, this);
		group->addButton(item);
		layout->addWidget(item);
		*button = item;
		return item;
	};

	add_section(QStringLiteral("游戏库"));
	add_nav(QStringLiteral("游戏库"), QStringLiteral(":/icons/grid-view.svg"), &m_nav_library);
	add_nav(QStringLiteral("收藏夹"), QStringLiteral(":/icons/star.svg"), &m_nav_favorites);
	add_nav(QStringLiteral("存档与补丁"), QStringLiteral(":/icons/archive.svg"), &m_nav_saves);
	add_nav(QStringLiteral("兼容性列表"), QStringLiteral(":/icons/shield.svg"),
	        &m_nav_compatibility);

	add_section(QStringLiteral("工具"));
	m_nav_input = MakeNavButton(QStringLiteral("输入映射"),
	                            QStringLiteral(":/icons/input-mapping.svg"), this);
	m_nav_input->setCheckable(false);
	layout->addWidget(m_nav_input);

	add_section(QStringLiteral("游戏目录"));
	m_dirs_host = new QWidget(sidebar);
	m_dirs_host->setObjectName(QStringLiteral("Transparent"));
	m_dirs_layout = new QVBoxLayout(m_dirs_host);
	m_dirs_layout->setContentsMargins(0, 0, 0, 0);
	m_dirs_layout->setSpacing(2);
	layout->addWidget(m_dirs_host);

	layout->addStretch(1);

	auto* runtime = new QFrame(sidebar);
	runtime->setObjectName(QStringLiteral("RuntimeCard"));
	auto* runtime_layout = new QVBoxLayout(runtime);
	runtime_layout->setContentsMargins(12, 12, 12, 12);
	runtime_layout->setSpacing(7);

	auto* runtime_top = new QHBoxLayout;
	runtime_top->setContentsMargins(0, 0, 0, 0);
	runtime_top->setSpacing(7);
	m_runtime_dot = new QLabel(runtime);
	m_runtime_dot->setObjectName(QStringLiteral("RuntimeDot"));
	m_runtime_dot->setFixedSize(8, 8);
	runtime_top->addWidget(m_runtime_dot);
	m_runtime_title = new QLabel(runtime);
	m_runtime_title->setObjectName(QStringLiteral("RuntimeTitle"));
	runtime_top->addWidget(m_runtime_title, 1);
	runtime_layout->addLayout(runtime_top);

	m_runtime_meta = new QLabel(runtime);
	m_runtime_meta->setObjectName(QStringLiteral("RuntimeMeta"));
	m_runtime_meta->setWordWrap(true);
	runtime_layout->addWidget(m_runtime_meta);

	m_runtime_detail = new QLabel(runtime);
	m_runtime_detail->setObjectName(QStringLiteral("RuntimeDetail"));
	runtime_layout->addWidget(m_runtime_detail);
	layout->addWidget(runtime);

	add_nav(QStringLiteral("设置"), QStringLiteral(":/icons/global-settings.svg"),
	        &m_nav_settings);

	connect(m_nav_library, &QPushButton::clicked, this, [this]() {
		m_directory_filter.clear();
		m_library->SetDirectoryFilter({});
		UpdateDirectoryChecks();
		NavigateTo(Section::Library);
	});
	connect(m_nav_favorites, &QPushButton::clicked, this, [this]() {
		m_directory_filter.clear();
		m_library->SetDirectoryFilter({});
		UpdateDirectoryChecks();
		NavigateTo(Section::Favorites);
	});
	connect(m_nav_saves, &QPushButton::clicked, this, [this]() { NavigateTo(Section::Saves); });
	connect(m_nav_compatibility, &QPushButton::clicked, this, [this]() {
		m_directory_filter.clear();
		m_library->SetDirectoryFilter({});
		UpdateDirectoryChecks();
		NavigateTo(Section::Compatibility);
	});
	connect(m_nav_settings, &QPushButton::clicked, this, [this]() { NavigateTo(Section::Settings); });
	connect(m_nav_input, &QPushButton::clicked, this,
	        [this]() { m_controller->EditInputMapping(); });

	return sidebar;
}

void LauncherShell::NavigateTo(Section section) {
	m_section = section;
	switch (section) {
		case Section::Library:
			m_library->SetMode(LibraryPage::Mode::Library);
			m_pages->setCurrentWidget(m_library);
			m_nav_library->setChecked(true);
			break;
		case Section::Favorites:
			m_library->SetMode(LibraryPage::Mode::Favorites);
			m_pages->setCurrentWidget(m_library);
			m_nav_favorites->setChecked(true);
			break;
		case Section::Compatibility:
			m_library->SetMode(LibraryPage::Mode::Compatibility);
			m_library->SetListMode(true);
			m_pages->setCurrentWidget(m_library);
			m_nav_compatibility->setChecked(true);
			break;
		case Section::Saves:
			m_saves->Refresh();
			m_pages->setCurrentWidget(m_saves);
			m_nav_saves->setChecked(true);
			break;
		case Section::Settings:
			m_settings->Refresh();
			m_pages->setCurrentWidget(m_settings);
			m_nav_settings->setChecked(true);
			break;
	}
	UpdateNavigationChecks();
}

void LauncherShell::SetDirectoryFilter(const QString& directory) {
	m_directory_filter = directory;
	m_library->SetDirectoryFilter(directory);
	UpdateDirectoryChecks();
	NavigateTo(Section::Library);
	SaveUiSettings();
}

void LauncherShell::UpdateSidebarIcons() {
	const auto recolor = [](QPushButton* button, const QString& icon, bool checked) {
		if (button == nullptr) {
			return;
		}
		button->setIcon(LauncherUi::TintedIcon(
		    icon, checked ? QColor(QStringLiteral("#3D8BFF")) : QColor(QStringLiteral("#9BA4B8")),
		    QSize(18, 18)));
	};
	recolor(m_nav_library, QStringLiteral(":/icons/grid-view.svg"), m_nav_library->isChecked());
	recolor(m_nav_favorites, QStringLiteral(":/icons/star.svg"), m_nav_favorites->isChecked());
	recolor(m_nav_saves, QStringLiteral(":/icons/archive.svg"), m_nav_saves->isChecked());
	recolor(m_nav_compatibility, QStringLiteral(":/icons/shield.svg"),
	        m_nav_compatibility->isChecked());
	recolor(m_nav_settings, QStringLiteral(":/icons/global-settings.svg"),
	        m_nav_settings->isChecked());
	recolor(m_nav_input, QStringLiteral(":/icons/input-mapping.svg"), false);
}

void LauncherShell::UpdateNavigationChecks() {
	UpdateSidebarIcons();
}

void LauncherShell::UpdateDirectoryChecks() {
	const auto buttons = m_dirs_host->findChildren<QPushButton*>(QStringLiteral("SidebarDir"));
	for (auto* button: buttons) {
		const auto path = button->property("path").toString();
		const QSignalBlocker blocker(button);
		button->setChecked(!m_directory_filter.isEmpty() &&
		                   NormalizePath(path) == NormalizePath(m_directory_filter));
	}
}

void LauncherShell::RefreshSidebar() {
	RefreshDirectoryButtons();
	UpdateRuntimeCard();
}

void LauncherShell::RefreshDirectoryButtons() {
	if (m_dirs_layout == nullptr) {
		return;
	}
	while (auto* item = m_dirs_layout->takeAt(0)) {
		if (auto* widget = item->widget(); widget != nullptr) {
			widget->deleteLater();
		}
		delete item;
	}

	const auto dirs = m_controller->GetGameDirectories();
	if (dirs.isEmpty()) {
		auto* empty = new QLabel(QStringLiteral("尚未添加目录"), m_dirs_host);
		empty->setObjectName(QStringLiteral("NavEmpty"));
		m_dirs_layout->addWidget(empty);
		return;
	}

	const auto items = m_controller->GetItems();
	for (const auto& dir: dirs) {
		int count = 0;
		for (auto* item: items) {
			const auto& info = item->GetInfo();
			auto        path = NormalizePath(info.basedir);
			auto        root = NormalizePath(dir);
#ifdef _WIN32
			path = path.toCaseFolded();
			root = root.toCaseFolded();
#endif
			if (path == root || path.startsWith(root + QLatin1Char('/'))) {
				count++;
			}
		}

		auto* button = new QPushButton(m_dirs_host);
		button->setObjectName(QStringLiteral("SidebarDir"));
		button->setCheckable(true);
		button->setFixedHeight(28);
		button->setCursor(Qt::PointingHandCursor);
		button->setProperty("path", dir);
		auto* layout = new QHBoxLayout(button);
		layout->setContentsMargins(8, 0, 8, 0);
		layout->setSpacing(6);

		auto* icon = new QLabel(button);
		icon->setPixmap(LauncherUi::TintedIcon(QStringLiteral(":/icons/folder.svg"),
		                                       QColor(QStringLiteral("#5F6A80")), QSize(14, 14))
		                    .pixmap(14, 14));
		icon->setAttribute(Qt::WA_TransparentForMouseEvents);
		layout->addWidget(icon);

		auto* path_label = new QLabel(button);
		path_label->setObjectName(QStringLiteral("SidebarDirPath"));
		path_label->setAttribute(Qt::WA_TransparentForMouseEvents);
		path_label->setText(LauncherUi::ElideText(path_label->font(), dir, 116));
		path_label->setToolTip(dir);
		layout->addWidget(path_label, 1);

		auto* count_label = new QLabel(button);
		count_label->setObjectName(QStringLiteral("SidebarDirCount"));
		count_label->setAttribute(Qt::WA_TransparentForMouseEvents);
		count_label->setText(QString::number(count));
		layout->addWidget(count_label);
		m_dirs_layout->addWidget(button);

		connect(button, &QPushButton::clicked, this, [this, dir, button]() {
			if (button->isChecked()) {
				SetDirectoryFilter(dir);
			} else {
				m_directory_filter.clear();
				m_library->SetDirectoryFilter({});
				UpdateDirectoryChecks();
				NavigateTo(Section::Library);
				SaveUiSettings();
			}
		});
	}
	UpdateDirectoryChecks();
}

void LauncherShell::UpdateRuntimeCard() {
	if (m_runtime_title == nullptr) {
		return;
	}
	const auto dirs  = m_controller->GetGameDirectories().size();
	const auto games = m_controller->GetItems().size();

	const bool ready = !m_runtime_api.isEmpty();
	m_runtime_title->setText(ready ? QStringLiteral("运行环境就绪")
	                               : QStringLiteral("Vulkan 未就绪"));
	if (m_runtime_dot != nullptr) {
		m_runtime_dot->setStyleSheet(
		    QStringLiteral("background-color: %1; border-radius: 4px;")
		        .arg(ready ? QStringLiteral("#37D399") : QStringLiteral("#F2B33D")));
	}
	m_runtime_meta->setText(ready ? QStringLiteral("%1 · %2").arg(m_runtime_api, m_runtime_device)
	                              : QStringLiteral("未检测到 Vulkan 1.3 设备"));
	m_runtime_detail->setText(
	    QStringLiteral("%1 个目录 · %2 个游戏").arg(dirs).arg(games));
}

void LauncherShell::SetVersion(const QString& version, const QString& detail) {
	const auto suffix = version.startsWith(QLatin1Char('v')) ? version.mid(1) : version;
	if (m_version_chip != nullptr) {
		m_version_chip->setText(QStringLiteral("v%1").arg(suffix));
	}
	if (m_settings != nullptr) {
		m_settings->SetVersion(suffix, detail);
	}
}

void LauncherShell::SetEmulatorVersion(const QString& version, const QString& detail) {
	SetVersion(version, detail);
}

void LauncherShell::SetEmulatorPath(const QString& path) {
	if (m_settings != nullptr) {
		m_settings->SetEmulatorPath(path);
	}
}

void LauncherShell::SetRuntimeInfo(const QString& device, const QString& api) {
	m_runtime_device = device;
	m_runtime_api    = api;
	if (m_settings != nullptr) {
		m_settings->SetRuntimeInfo(device, api);
	}
	UpdateRuntimeCard();
}

void LauncherShell::SetControllerName(const QString& name) {
	m_controller_name = name;
	if (m_settings != nullptr) {
		m_settings->SetControllerName(name);
	}
}

void LauncherShell::SetCheckOnStartup(bool checked) {
	if (m_settings != nullptr) {
		m_settings->SetCheckOnStartup(checked);
	}
}

void LauncherShell::SetCheckUpdatesSupported(bool supported) {
	if (m_settings != nullptr) {
		m_settings->SetCheckUpdatesSupported(supported);
	}
}

void LauncherShell::SetCheckingForUpdates(bool checking) {
	if (m_settings != nullptr) {
		m_settings->SetCheckingForUpdates(checking);
	}
}

void LauncherShell::SetInstalling(bool installing) {
	if (m_settings != nullptr) {
		m_settings->SetInstalling(installing);
	}
}

void LauncherShell::SetRunEnabled(bool enabled) {
	m_run_enabled = enabled;
	if (m_library != nullptr) {
		m_library->SetRunEnabled(enabled);
	}
}

void LauncherShell::RecordLastPlayed() {
	auto* item = m_controller->GetSelectedItem();
	if (item == nullptr) {
		return;
	}
	const auto& info = item->GetInfo();
	const auto  path = !info.game_path.isEmpty() ? info.game_path : info.basedir;
	if (path.isEmpty()) {
		return;
	}
	m_last_played_path = NormalizePath(path);
	m_last_played_time = QDateTime::currentDateTime();
	m_library->SetLastPlayed(m_last_played_path, m_last_played_time);
	SaveUiSettings();
}

void LauncherShell::LoadUiSettings() {
	const auto path = m_controller->GetSettingsFile();
	QSettings  settings(path, QSettings::IniFormat);
	settings.beginGroup(QStringLiteral("LauncherUi"));

	const auto favorites = settings.value(QStringLiteral("favorites")).toStringList();
	m_favorites.clear();
	for (const auto& favorite: favorites) {
		m_favorites.insert(favorite);
	}
	m_last_played_path = settings.value(QStringLiteral("last_played_path")).toString();
	m_last_played_time = settings.value(QStringLiteral("last_played_time")).toDateTime();
	m_directory_filter = settings.value(QStringLiteral("directory_filter")).toString();
	const bool list_mode = settings.value(QStringLiteral("list_mode"), false).toBool();
	const int  grid_columns = settings.value(QStringLiteral("grid_columns"), 0).toInt();
	settings.endGroup();

	m_library->SetFavorites(m_favorites);
	m_library->SetLastPlayed(m_last_played_path, m_last_played_time);
	m_library->SetListMode(list_mode);
	m_library->SetGridColumns(grid_columns);
	m_settings->SetGridColumns(grid_columns);

	if (!m_directory_filter.isEmpty()) {
		bool exists = false;
		for (const auto& dir: m_controller->GetGameDirectories()) {
			if (NormalizePath(dir) == NormalizePath(m_directory_filter)) {
				exists = true;
				break;
			}
		}
		if (exists) {
			m_library->SetDirectoryFilter(m_directory_filter);
		} else {
			m_directory_filter.clear();
		}
	}
	UpdateDirectoryChecks();
	SaveUiSettings();
}

void LauncherShell::SaveUiSettings() {
	if (!m_ui_loaded) {
		return;
	}
	const auto path = m_controller->GetSettingsFile();
	if (path.isEmpty()) {
		return;
	}
	QSettings settings(path, QSettings::IniFormat);
	settings.beginGroup(QStringLiteral("LauncherUi"));
	QStringList favorites;
	for (const auto& favorite: m_favorites) {
		favorites.append(favorite);
	}
	settings.setValue(QStringLiteral("favorites"), favorites);
	settings.setValue(QStringLiteral("last_played_path"), m_last_played_path);
	settings.setValue(QStringLiteral("last_played_time"), m_last_played_time);
	settings.setValue(QStringLiteral("directory_filter"), m_directory_filter);
	settings.setValue(QStringLiteral("list_mode"), m_library != nullptr && m_library->IsListMode());
	settings.setValue(QStringLiteral("grid_columns"), m_library != nullptr ? m_library->GetGridColumns() : 0);
	settings.endGroup();
}

void LauncherShell::ResetUi() {
	const auto answer = QMessageBox::question(
	    this, QStringLiteral("恢复默认设置"),
	    QStringLiteral("将恢复启动器的外观与浏览设置（不会删除游戏目录或游戏配置）。继续吗？"),
	    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if (answer != QMessageBox::Yes) {
		return;
	}

	const auto path = m_controller->GetSettingsFile();
	if (!path.isEmpty()) {
		QSettings settings(path, QSettings::IniFormat);
		settings.remove(QStringLiteral("LauncherUi"));
	}

	m_favorites.clear();
	m_last_played_path.clear();
	m_last_played_time = {};
	m_directory_filter.clear();
	m_library->SetFavorites({});
	m_library->SetLastPlayed({}, {});
	m_library->SetDirectoryFilter({});
	m_library->SetListMode(false);
	m_library->ResetUiState();
	UpdateDirectoryChecks();
	NavigateTo(Section::Library);
	m_settings->Refresh();
	SaveUiSettings();
}

void LauncherShell::showEvent(QShowEvent* event) {
	QWidget::showEvent(event);

	for (auto* label: findChildren<QLabel*>(QStringLiteral("NavSection"))) {
		auto font = label->font();
		font.setLetterSpacing(QFont::PercentageSpacing, 102);
		label->setFont(font);
	}
	if (auto* handle = window()->windowHandle(); handle != nullptr) {
		connect(handle, &QWindow::windowStateChanged, this,
		        [this](Qt::WindowState) {
			        if (m_maximize_button != nullptr) {
				        m_maximize_button->setText(window()->isMaximized()
				                                       ? QStringLiteral("\u2750")
				                                       : QStringLiteral("\u25A1"));
			        }
		        });
	}
}
