#include "settingsPage.h"

#include "configuration.h"
#include "configurationListWidget.h"
#include "configurationItem.h"
#include "gameCards.h"
#include "gameContent.h"
#include "launcherUiUtils.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QSlider>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString FormatResolution(const QString& resolution) {
	auto text = Configuration::NormalizeResolution(resolution);
	if (text.isEmpty()) {
		text = resolution;
	}
	text.replace(QLatin1Char('x'), QStringLiteral(" × "));
	text.replace(QLatin1Char('X'), QStringLiteral(" × "));
	return text;
}

QString GridColumnsText(int columns) {
	return columns <= 0 ? QStringLiteral("自动") : QStringLiteral("%1 列").arg(columns);
}

bool PathUnder(const QString& path, const QString& directory) {
	auto normalized_path = QDir::cleanPath(path);
	auto normalized_dir  = QDir::cleanPath(directory);
#ifdef _WIN32
	normalized_path = normalized_path.toCaseFolded();
	normalized_dir  = normalized_dir.toCaseFolded();
#endif
	if (normalized_path == normalized_dir) {
		return true;
	}
	if (!normalized_dir.endsWith(QLatin1Char('/'))) {
		normalized_dir += QLatin1Char('/');
	}
	return normalized_path.startsWith(normalized_dir);
}

} // namespace

SettingsPage::SettingsPage(ConfigurationListWidget* controller, QWidget* parent)
    : QWidget(parent), m_controller(controller) {
	BuildUi();
	connect(m_controller, &ConfigurationListWidget::ItemsChanged, this,
	        [this]() { Refresh(); });
	Refresh();
}

void SettingsPage::BuildUi() {
	auto* root = new QVBoxLayout(this);
	root->setContentsMargins(32, 24, 32, 24);
	root->setSpacing(14);

	auto* title = new QLabel(QStringLiteral("设置"), this);
	title->setObjectName(QStringLiteral("PageTitle"));
	auto* subtitle = new QLabel(this);
	subtitle->setObjectName(QStringLiteral("PageSubtitle"));
	const auto settings_name = QFileInfo(m_controller->GetSettingsFile()).fileName();
	subtitle->setText(QStringLiteral("外观、图形、路径与更新 · 修改后即时写入 %1")
	                      .arg(settings_name.isEmpty() ? QStringLiteral("Kyty.ini")
	                                                   : settings_name));

	auto* title_layout = new QVBoxLayout;
	title_layout->setContentsMargins(0, 0, 0, 0);
	title_layout->setSpacing(3);
	title_layout->addWidget(title);
	title_layout->addWidget(subtitle);

	auto* global_button = new QPushButton(QStringLiteral("全局设置"), this);
	global_button->setObjectName(QStringLiteral("SecondaryButton"));
	global_button->setFixedHeight(34);
	global_button->setCursor(Qt::PointingHandCursor);
	connect(global_button, &QPushButton::clicked, this, [this]() {
		m_controller->EditGlobalSettings();
		Refresh();
	});

	auto* reset_button = new QPushButton(QStringLiteral("恢复默认设置"), this);
	reset_button->setObjectName(QStringLiteral("SecondaryButton"));
	reset_button->setFixedHeight(34);
	reset_button->setCursor(Qt::PointingHandCursor);
	connect(reset_button, &QPushButton::clicked, this, &SettingsPage::ResetRequested);

	auto* header = new QHBoxLayout;
	header->setContentsMargins(0, 0, 0, 0);
	header->setSpacing(8);
	header->addLayout(title_layout, 1);
	header->addWidget(global_button, 0, Qt::AlignVCenter);
	header->addWidget(reset_button, 0, Qt::AlignVCenter);
	root->addLayout(header);

	auto* tabs_host = new QWidget(this);
	tabs_host->setObjectName(QStringLiteral("SettingsTabs"));
	auto* tabs = new QHBoxLayout(tabs_host);
	tabs->setContentsMargins(0, 0, 0, 0);
	tabs->setSpacing(6);

	auto* group = new QButtonGroup(this);
	group->setExclusive(true);
	const auto add_tab = [this, tabs, group](const QString& text, QPushButton** button) {
		auto* tab = new QPushButton(text, this);
		tab->setObjectName(QStringLiteral("SectionTab"));
		tab->setCheckable(true);
		tab->setCursor(Qt::PointingHandCursor);
		tab->setFixedHeight(30);
		group->addButton(tab);
		tabs->addWidget(tab);
		*button = tab;
	};
	add_tab(QStringLiteral("通用"), &m_tab_general);
	add_tab(QStringLiteral("图形"), &m_tab_graphics);
	add_tab(QStringLiteral("音频"), &m_tab_audio);
	add_tab(QStringLiteral("控制器"), &m_tab_controller);
	add_tab(QStringLiteral("存储路径"), &m_tab_storage);
	add_tab(QStringLiteral("更新"), &m_tab_update);
	add_tab(QStringLiteral("兼容性"), &m_tab_compatibility);
	m_tab_general->setChecked(true);
	tabs->addStretch(1);
	root->addWidget(tabs_host);

	m_scroll = new QScrollArea(this);
	m_scroll->setObjectName(QStringLiteral("SettingsScroll"));
	m_scroll->setFrameShape(QFrame::NoFrame);
	m_scroll->setWidgetResizable(true);
	m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	m_scroll_content = new QWidget(m_scroll);
	m_scroll_content->setObjectName(QStringLiteral("Transparent"));
	auto* columns = new QHBoxLayout(m_scroll_content);
	columns->setContentsMargins(0, 0, 0, 0);
	columns->setSpacing(16);

	m_left_column  = new QVBoxLayout;
	m_right_column = new QVBoxLayout;
	m_left_column->setContentsMargins(0, 0, 0, 0);
	m_right_column->setContentsMargins(0, 0, 0, 0);
	m_left_column->setSpacing(16);
	m_right_column->setSpacing(16);
	columns->addLayout(m_left_column, 1);
	columns->addLayout(m_right_column, 1);

	QVBoxLayout* body = nullptr;

	m_save_timer = new QTimer(this);
	m_save_timer->setSingleShot(true);
	m_save_timer->setInterval(250);
	connect(m_save_timer, &QTimer::timeout, this, [this]() { m_controller->WriteSettings(); });

	m_appearance_card = AddCard(m_left_column, QStringLiteral("外观与语言"), &body);
	m_theme_value     = AddTextRow(body, QStringLiteral("主题"));
	m_language_value  = AddTextRow(body, QStringLiteral("界面语言"));
	m_scale_value     = AddTextRow(body, QStringLiteral("界面缩放"));
	m_theme_value->setText(QStringLiteral("深色"));
	m_language_value->setText(QStringLiteral("简体中文"));

	auto* grid_widget = new QWidget(m_appearance_card);
	grid_widget->setObjectName(QStringLiteral("Transparent"));
	auto* grid_layout = new QHBoxLayout(grid_widget);
	grid_layout->setContentsMargins(0, 0, 0, 0);
	grid_layout->setSpacing(8);
	m_grid_slider = new QSlider(Qt::Horizontal, grid_widget);
	m_grid_slider->setObjectName(QStringLiteral("SettingsSlider"));
	m_grid_slider->setRange(0, 5);
	m_grid_slider->setPageStep(1);
	m_grid_slider->setFixedWidth(120);
	m_grid_slider->setToolTip(QStringLiteral("自动按窗口宽度排布；列数越多，封面越小。"));
	m_grid_value = new QLabel(grid_widget);
	m_grid_value->setObjectName(QStringLiteral("SettingsValue"));
	m_grid_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	m_grid_value->setFixedWidth(48);
	m_grid_value->setText(GridColumnsText(0));
	grid_layout->addWidget(m_grid_slider);
	grid_layout->addWidget(m_grid_value);
	AddRow(body, QStringLiteral("网格封面尺寸"), grid_widget);
	connect(m_grid_slider, &QSlider::valueChanged, this, [this](int value) {
		if (m_updating) {
			return;
		}
		m_grid_value->setText(GridColumnsText(value));
		emit GridColumnsChanged(value);
	});

	auto* appearance_note = new QLabel(
	    QStringLiteral("启动器外观固定为深色，界面缩放跟随系统；网格封面尺寸可调。"),
	    m_appearance_card);
	appearance_note->setObjectName(QStringLiteral("DetailNote"));
	appearance_note->setWordWrap(true);
	body->addWidget(appearance_note);

	m_storage_card = AddCard(m_left_column, QStringLiteral("存储路径"), &body);
	m_dirs_host    = new QWidget(m_storage_card);
	m_dirs_host->setObjectName(QStringLiteral("Transparent"));
	m_dirs_layout = new QVBoxLayout(m_dirs_host);
	m_dirs_layout->setContentsMargins(0, 0, 0, 0);
	m_dirs_layout->setSpacing(10);
	body->addWidget(m_dirs_host);

	auto* add_dir = new QPushButton(QStringLiteral("+ 添加游戏目录"), m_storage_card);
	add_dir->setObjectName(QStringLiteral("SecondaryButton"));
	add_dir->setFixedHeight(34);
	add_dir->setCursor(Qt::PointingHandCursor);
	connect(add_dir, &QPushButton::clicked, this, [this]() {
		m_controller->AddGameDirectoryInteractive(this);
		Refresh();
	});
	body->addWidget(add_dir);

	m_audio_card = AddCard(m_left_column, QStringLiteral("音频"), &body);
	m_audio_backend_value = AddTextRow(body, QStringLiteral("音频后端"));
	m_audio_backend_value->setText(LauncherUi::AudioBackendName());

	m_audio_output_combo = new QComboBox(m_audio_card);
	m_audio_output_combo->setObjectName(QStringLiteral("SettingsCombo"));
	m_audio_output_combo->setFixedHeight(30);
	m_audio_output_combo->setMinimumWidth(220);
	m_audio_output_combo->addItem(QStringLiteral("系统默认"), QString {});
	for (const auto& device: LauncherUi::QueryAudioPlaybackDevices()) {
		m_audio_output_combo->addItem(device, device);
	}
	AddRow(body, QStringLiteral("输出设备"), m_audio_output_combo);
	connect(m_audio_output_combo, &QComboBox::currentIndexChanged, this, [this](int) {
		if (m_updating) {
			return;
		}
		auto& global              = m_controller->GetGlobalInfo();
		global.audio_output_device = m_audio_output_combo->currentData().toString();
		m_controller->WriteSettings();
	});

	auto* volume_widget = new QWidget(m_audio_card);
	volume_widget->setObjectName(QStringLiteral("Transparent"));
	auto* volume_layout = new QHBoxLayout(volume_widget);
	volume_layout->setContentsMargins(0, 0, 0, 0);
	volume_layout->setSpacing(8);
	m_volume_slider = new QSlider(Qt::Horizontal, volume_widget);
	m_volume_slider->setObjectName(QStringLiteral("SettingsSlider"));
	m_volume_slider->setRange(0, 100);
	m_volume_slider->setPageStep(10);
	m_volume_slider->setFixedWidth(120);
	m_volume_value = new QLabel(volume_widget);
	m_volume_value->setObjectName(QStringLiteral("SettingsValue"));
	m_volume_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	m_volume_value->setFixedWidth(36);
	volume_layout->addWidget(m_volume_slider);
	volume_layout->addWidget(m_volume_value);
	AddRow(body, QStringLiteral("主音量"), volume_widget);
	connect(m_volume_slider, &QSlider::valueChanged, this, [this](int value) {
		if (m_updating) {
			return;
		}
		m_volume_value->setText(QStringLiteral("%1%").arg(value));
		auto& global       = m_controller->GetGlobalInfo();
		global.audio_volume = value;
		if (m_save_timer != nullptr) {
			m_save_timer->start();
		}
	});

	auto* audio_note = new QLabel(
	    QStringLiteral("主音量是模拟器的游戏音频增益；系统音量仍由 Windows 控制。修改在下次启动游戏时生效。"),
	    m_audio_card);
	audio_note->setObjectName(QStringLiteral("DetailNote"));
	audio_note->setWordWrap(true);
	body->addWidget(audio_note);

	m_graphics_card = AddCard(m_right_column, QStringLiteral("图形"), &body);
	m_backend_value = AddTextRow(body, QStringLiteral("图形后端"));

	m_device_combo = new QComboBox(m_graphics_card);
	m_device_combo->setObjectName(QStringLiteral("SettingsCombo"));
	m_device_combo->setFixedHeight(30);
	m_device_combo->addItem(QStringLiteral("自动选择"), -1);
	const auto devices = LauncherUi::QueryVulkanDevices();
	for (int index = 0; index < devices.size(); index++) {
		m_device_combo->addItem(devices.at(index).name, index);
	}
	AddRow(body, QStringLiteral("渲染设备"), m_device_combo);
	connect(m_device_combo, &QComboBox::currentIndexChanged, this, [this](int) {
		if (m_updating) {
			return;
		}
		auto& global     = m_controller->GetGlobalInfo();
		global.gpu_index = m_device_combo->currentData().toInt();
		m_controller->WriteSettings();
	});

	m_resolution_combo = new QComboBox(m_graphics_card);
	m_resolution_combo->setObjectName(QStringLiteral("SettingsCombo"));
	m_resolution_combo->setFixedHeight(30);
	for (const auto& resolution:
	     {QStringLiteral("1280x720"), QStringLiteral("1920x1080"),
	      QStringLiteral("2560x1440"), QStringLiteral("3840x2160")}) {
		m_resolution_combo->addItem(FormatResolution(resolution), resolution);
	}
	AddRow(body, QStringLiteral("内部分辨率"), m_resolution_combo);
	connect(m_resolution_combo, &QComboBox::currentIndexChanged, this, [this](int) {
		if (m_updating) {
			return;
		}
		auto& global            = m_controller->GetGlobalInfo();
		global.screen_resolution = m_resolution_combo->currentData().toString();
		m_controller->WriteSettings();
	});

	m_stretch_toggle = new ToggleSwitch(m_graphics_card);
	m_stretch_toggle->setToolTip(
	    QStringLiteral("非 16:9 分辨率时拉伸画面填满窗口，而不是显示黑边。"));
	AddRow(body, QStringLiteral("拉伸到窗口"), m_stretch_toggle);
	connect(m_stretch_toggle, &ToggleSwitch::Toggled, this, [this](bool checked) {
		if (m_updating) {
			return;
		}
		auto& global            = m_controller->GetGlobalInfo();
		global.stretch_enabled  = checked;
		m_controller->WriteSettings();
	});

	m_shader_cache_toggle = new ToggleSwitch(m_graphics_card);
	m_shader_cache_toggle->setToolTip(
	    QStringLiteral("缓存已编译的着色器和管线；关闭后每次启动都需要重新编译。"));
	AddRow(body, QStringLiteral("缓存着色器"), m_shader_cache_toggle);
	connect(m_shader_cache_toggle, &ToggleSwitch::Toggled, this, [this](bool checked) {
		if (m_updating) {
			return;
		}
		auto& global                = m_controller->GetGlobalInfo();
		global.shader_cache_enabled = checked;
		m_controller->WriteSettings();
	});

	m_update_card = AddCard(m_right_column, QStringLiteral("更新"), &body);
	m_version_value = AddTextRow(body, QStringLiteral("当前版本"));
	m_version_value->setText(QStringLiteral("v0.0.0"));
	m_emulator_path = AddTextRow(body, QStringLiteral("模拟器路径"));

	auto* switch_row = new QHBoxLayout;
	switch_row->setContentsMargins(0, 0, 0, 0);
	auto* switch_label = new QLabel(QStringLiteral("启动时检查更新"), m_update_card);
	switch_label->setObjectName(QStringLiteral("SettingsLabel"));
	switch_row->addWidget(switch_label);
	switch_row->addStretch(1);
	m_check_on_startup = new ToggleSwitch(m_update_card);
	switch_row->addWidget(m_check_on_startup, 0, Qt::AlignRight | Qt::AlignVCenter);
	body->addLayout(switch_row);
	connect(m_check_on_startup, &ToggleSwitch::Toggled, this,
	        &SettingsPage::CheckOnStartupChanged);

	m_check_updates = new QPushButton(QStringLiteral("检查更新"), m_update_card);
	m_check_updates->setObjectName(QStringLiteral("SecondaryButton"));
	m_check_updates->setFixedHeight(34);
	m_check_updates->setCursor(Qt::PointingHandCursor);
	connect(m_check_updates, &QPushButton::clicked, this,
	        &SettingsPage::CheckForUpdatesRequested);
	body->addWidget(m_check_updates);

	m_update_status = new QLabel(m_update_card);
	m_update_status->setObjectName(QStringLiteral("DetailNote"));
	m_update_status->setWordWrap(true);
	body->addWidget(m_update_status);

	m_compatibility_card = AddCard(m_right_column, QStringLiteral("兼容性"), &body);

	m_psn_toggle = new ToggleSwitch(m_compatibility_card);
	m_psn_toggle->setToolTip(
	    QStringLiteral("模拟 PSN 登录；用于需要 PSN 服务的离线游戏。"));
	AddRow(body, QStringLiteral("PSN 模拟"), m_psn_toggle);
	connect(m_psn_toggle, &ToggleSwitch::Toggled, this, [this](bool checked) {
		if (m_updating) {
			return;
		}
		auto& global                   = m_controller->GetGlobalInfo();
		global.psn_emulation_enabled   = checked;
		m_controller->WriteSettings();
	});

	m_pro_toggle = new ToggleSwitch(m_compatibility_card);
	m_pro_toggle->setToolTip(
	    QStringLiteral("实验性：启用 Pro/Trinity 渲染路径；仅部分游戏支持。"));
	AddRow(body, QStringLiteral("Pro/Trinity 渲染（实验性）"), m_pro_toggle);
	connect(m_pro_toggle, &ToggleSwitch::Toggled, this, [this](bool checked) {
		if (m_updating) {
			return;
		}
		auto& global       = m_controller->GetGlobalInfo();
		global.pro_enabled = checked;
		m_controller->WriteSettings();
	});

#if defined(_WIN32)
	m_red_zone_toggle = new ToggleSwitch(m_compatibility_card);
	m_red_zone_toggle->setToolTip(
	    QStringLiteral("实验性：防止部分游戏在角色创建或场景切换时崩溃。"));
	AddRow(body, QStringLiteral("Windows SysV 红区崩溃保护"), m_red_zone_toggle);
	connect(m_red_zone_toggle, &ToggleSwitch::Toggled, this, [this](bool checked) {
		if (m_updating) {
			return;
		}
		auto& global = m_controller->GetGlobalInfo();
		global.red_zone_protection_enabled = checked;
		m_controller->WriteSettings();
	});

	m_close_terminal_toggle = new ToggleSwitch(m_compatibility_card);
	m_close_terminal_toggle->setToolTip(
	    QStringLiteral("游戏正常退出后关闭启动器打开的控制台窗口。"));
	AddRow(body, QStringLiteral("游戏退出后关闭终端"), m_close_terminal_toggle);
	connect(m_close_terminal_toggle, &ToggleSwitch::Toggled, this, [this](bool checked) {
		if (m_updating) {
			return;
		}
		auto& global          = m_controller->GetGlobalInfo();
		global.close_terminal = checked;
		m_controller->WriteSettings();
	});
#endif

	auto* compatibility_note = new QLabel(
	    QStringLiteral("PSN 模拟和 Pro/Trinity 为实验性选项；修改在下次启动游戏时生效。"),
	    m_compatibility_card);
	compatibility_note->setObjectName(QStringLiteral("DetailNote"));
	compatibility_note->setWordWrap(true);
	body->addWidget(compatibility_note);

	m_controller_card = AddCard(m_right_column, QStringLiteral("控制器"), &body);
	m_pad_value     = AddTextRow(body, QStringLiteral("已连接设备"));
	m_trigger_value = AddTextRow(body, QStringLiteral("自适应扳机"));
	m_pad_value->setText(QStringLiteral("自动检测"));
	m_trigger_value->setText(QStringLiteral("已启用"));

	auto* speaker_widget = new QWidget(m_controller_card);
	speaker_widget->setObjectName(QStringLiteral("Transparent"));
	auto* speaker_layout = new QHBoxLayout(speaker_widget);
	speaker_layout->setContentsMargins(0, 0, 0, 0);
	speaker_layout->setSpacing(8);
	m_speaker_volume_slider = new QSlider(Qt::Horizontal, speaker_widget);
	m_speaker_volume_slider->setObjectName(QStringLiteral("SettingsSlider"));
	m_speaker_volume_slider->setRange(0, 100);
	m_speaker_volume_slider->setPageStep(10);
	m_speaker_volume_slider->setFixedWidth(120);
	m_speaker_volume_slider->setToolTip(QStringLiteral("DualSense 手柄扬声器音量，不影响游戏主输出。"));
	m_speaker_volume_value = new QLabel(speaker_widget);
	m_speaker_volume_value->setObjectName(QStringLiteral("SettingsValue"));
	m_speaker_volume_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	m_speaker_volume_value->setFixedWidth(36);
	speaker_layout->addWidget(m_speaker_volume_slider);
	speaker_layout->addWidget(m_speaker_volume_value);
	AddRow(body, QStringLiteral("手柄扬声器音量"), speaker_widget);
	connect(m_speaker_volume_slider, &QSlider::valueChanged, this, [this](int value) {
		if (m_updating) {
			return;
		}
		m_speaker_volume_value->setText(QStringLiteral("%1%").arg(value));
		auto& global = m_controller->GetGlobalInfo();
		global.controller.speaker_volume = value;
		if (m_save_timer != nullptr) {
			m_save_timer->start();
		}
	});

	auto* mapping_widget = new QWidget(m_controller_card);
	mapping_widget->setObjectName(QStringLiteral("Transparent"));
	auto* mapping_layout = new QHBoxLayout(mapping_widget);
	mapping_layout->setContentsMargins(0, 0, 0, 0);
	mapping_layout->setSpacing(8);
	m_mapping_value = new QLabel(mapping_widget);
	m_mapping_value->setObjectName(QStringLiteral("SettingsValue"));
	m_mapping_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	mapping_layout->addWidget(m_mapping_value);
	auto* edit_mapping = new QPushButton(QStringLiteral("编辑"), mapping_widget);
	edit_mapping->setObjectName(QStringLiteral("LinkButton"));
	edit_mapping->setFlat(true);
	edit_mapping->setFixedHeight(20);
	edit_mapping->setCursor(Qt::PointingHandCursor);
	mapping_layout->addWidget(edit_mapping);
	AddRow(body, QStringLiteral("输入映射"), mapping_widget);
	connect(edit_mapping, &QPushButton::clicked, this, [this]() {
		m_controller->EditInputMapping();
		Refresh();
	});


	m_scroll->setWidget(m_scroll_content);
	root->addWidget(m_scroll, 1);

	auto* footer = new QFrame(this);
	footer->setObjectName(QStringLiteral("SettingsFooter"));
	footer->setFixedHeight(40);
	auto* footer_layout = new QHBoxLayout(footer);
	footer_layout->setContentsMargins(14, 0, 14, 0);
	footer_layout->setSpacing(8);
	m_config_path = new QLabel(footer);
	m_config_path->setObjectName(QStringLiteral("FooterText"));
	footer_layout->addWidget(m_config_path, 1);
	auto* open_folder = new QPushButton(QStringLiteral("打开所在文件夹"), footer);
	open_folder->setObjectName(QStringLiteral("LinkButton"));
	open_folder->setFlat(true);
	open_folder->setFixedHeight(24);
	open_folder->setCursor(Qt::PointingHandCursor);
	connect(open_folder, &QPushButton::clicked, this,
	        [this]() { m_controller->OpenSettingsFileFolder(); });
	footer_layout->addWidget(open_folder);
	root->addWidget(footer);

	connect(m_tab_general, &QPushButton::clicked, this, [this]() { SetSection(0); });
	connect(m_tab_graphics, &QPushButton::clicked, this, [this]() { SetSection(1); });
	connect(m_tab_audio, &QPushButton::clicked, this, [this]() { SetSection(2); });
	connect(m_tab_controller, &QPushButton::clicked, this, [this]() { SetSection(3); });
	connect(m_tab_storage, &QPushButton::clicked, this, [this]() { SetSection(4); });
	connect(m_tab_update, &QPushButton::clicked, this, [this]() { SetSection(5); });
	connect(m_tab_compatibility, &QPushButton::clicked, this, [this]() { SetSection(6); });

	SetSection(0);
}

QWidget* SettingsPage::AddCard(QVBoxLayout* column, const QString& text, QVBoxLayout** body) {
	auto* card = new QFrame(m_scroll_content);
	card->setObjectName(QStringLiteral("SettingsCard"));
	auto* layout = new QVBoxLayout(card);
	layout->setContentsMargins(18, 16, 18, 16);
	layout->setSpacing(10);

	auto* title = new QLabel(text, card);
	title->setObjectName(QStringLiteral("SettingsCardTitle"));
	layout->addWidget(title);

	*body = layout;
	column->addWidget(card);
	return card;
}

void SettingsPage::AddRow(QVBoxLayout* body, const QString& label, QWidget* value) {
	auto* row = new QHBoxLayout;
	row->setContentsMargins(0, 0, 0, 0);
	row->setSpacing(8);
	auto* name = new QLabel(label, body->parentWidget());
	name->setObjectName(QStringLiteral("SettingsLabel"));
	name->setMinimumHeight(26);
	row->addWidget(name);
	row->addStretch(1);
	if (value != nullptr) {
		value->setMinimumHeight(26);
		row->addWidget(value, 0, Qt::AlignRight | Qt::AlignVCenter);
	}
	body->addLayout(row);
}

QLabel* SettingsPage::AddTextRow(QVBoxLayout* body, const QString& label) {
	auto* value = new QLabel(QStringLiteral("\u2014"), body->parentWidget());
	value->setObjectName(QStringLiteral("SettingsValue"));
	value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	AddRow(body, label, value);
	return value;
}

void SettingsPage::AddLinkRow(QVBoxLayout* body, const QString& label, QPushButton** button) {
	auto* link = new QPushButton(label, body->parentWidget());
	link->setObjectName(QStringLiteral("LinkButton"));
	link->setFlat(true);
	link->setCursor(Qt::PointingHandCursor);
	AddRow(body, {}, link);
	*button = link;
}

void SettingsPage::Refresh() {
	const auto& global = m_controller->GetGlobalInfo();

	const auto dpr = QGuiApplication::primaryScreen() != nullptr
	                     ? QGuiApplication::primaryScreen()->devicePixelRatio()
	                     : 1.0;
	m_scale_value->setText(QStringLiteral("100%"));
	m_scale_value->setToolTip(
	    QStringLiteral("系统缩放 %1%").arg(qRound(dpr * 100.0)));
	m_updating = true;
	{
		const QSignalBlocker blocker(m_resolution_combo);
		int index = m_resolution_combo->findData(global.screen_resolution);
		if (index < 0) {
			m_resolution_combo->addItem(FormatResolution(global.screen_resolution),
			                           global.screen_resolution);
			index = m_resolution_combo->count() - 1;
		}
		m_resolution_combo->setCurrentIndex(index);
	}
	{
		const QSignalBlocker blocker(m_device_combo);
		const int index = m_device_combo->findData(global.gpu_index);
		m_device_combo->setCurrentIndex(index >= 0 ? index : 0);
	}
	if (m_stretch_toggle != nullptr) {
		m_stretch_toggle->SetChecked(global.stretch_enabled);
	}
	if (m_shader_cache_toggle != nullptr) {
		m_shader_cache_toggle->SetChecked(global.shader_cache_enabled);
	}
	if (m_psn_toggle != nullptr) {
		m_psn_toggle->SetChecked(global.psn_emulation_enabled);
	}
	if (m_pro_toggle != nullptr) {
		m_pro_toggle->SetChecked(global.pro_enabled);
	}
#if defined(_WIN32)
	if (m_red_zone_toggle != nullptr) {
		m_red_zone_toggle->SetChecked(global.red_zone_protection_enabled);
	}
	if (m_close_terminal_toggle != nullptr) {
		m_close_terminal_toggle->SetChecked(global.close_terminal);
	}
#endif
	{
		const QSignalBlocker blocker(m_volume_slider);
		m_volume_slider->setValue(global.audio_volume);
		m_volume_value->setText(QStringLiteral("%1%").arg(global.audio_volume));
	}
	{
		const QSignalBlocker blocker(m_speaker_volume_slider);
		m_speaker_volume_slider->setValue(global.controller.speaker_volume);
		m_speaker_volume_value->setText(
		    QStringLiteral("%1%").arg(global.controller.speaker_volume));
	}
	{
		const QSignalBlocker blocker(m_audio_output_combo);
		int index = 0;
		if (!global.audio_output_device.isEmpty()) {
			index = m_audio_output_combo->findData(global.audio_output_device);
			if (index < 0) {
				m_audio_output_combo->addItem(
				    QStringLiteral("%1（不可用）").arg(global.audio_output_device),
				    global.audio_output_device);
				index = m_audio_output_combo->count() - 1;
			}
		}
		m_audio_output_combo->setCurrentIndex(index);
	}
	m_updating = false;
	m_mapping_value->setText(global.host_input_mapping.isEmpty() ? QStringLiteral("默认配置")
	                                                             : QStringLiteral("自定义"));
	m_pad_value->setText(m_controller_name.isEmpty() ? QStringLiteral("自动检测")
	                                                 : m_controller_name);
	m_backend_value->setText(m_graphics_api.isEmpty() ? QStringLiteral("自动")
	                                                  : m_graphics_api);
	m_update_status->setText(!m_check_updates_supported
	                             ? QStringLiteral("当前构建不支持自动更新")
	                             : (m_installing ? QStringLiteral("正在安装更新…")
	                                             : (m_checking ? QStringLiteral("正在检查更新…")
	                                                           : QStringLiteral("点击检查更新"))));
	m_check_updates->setEnabled(m_check_updates_supported && !m_checking && !m_installing);
	m_check_on_startup->setEnabled(m_check_updates_supported);
	if (m_version_value != nullptr && !m_version.isEmpty()) {
		m_version_value->setText(QStringLiteral("v%1").arg(m_version));
		m_version_value->setToolTip(m_version_detail);
	}

	const auto settings_path = m_controller->GetSettingsFile();
	m_config_path->setText(LauncherUi::ElideText(
	    m_config_path->font(), QStringLiteral("配置保存在 %1 · 修改在重启后生效").arg(settings_path),
	    760));
	m_config_path->setToolTip(settings_path);

	UpdatePaths();
}

void SettingsPage::UpdatePaths() {
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
		auto* empty = new QLabel(QStringLiteral("尚未添加游戏目录"), m_dirs_host);
		empty->setObjectName(QStringLiteral("SettingsValue"));
		m_dirs_layout->addWidget(empty);
	}

	const auto items = m_controller->GetItems();
	for (const auto& dir: dirs) {
		int count = 0;
		for (auto* item: items) {
			const auto& info = item->GetInfo();
			if (PathUnder(info.basedir, dir) || PathUnder(info.game_path, dir)) {
				count++;
			}
		}

		auto* row = new QWidget(m_dirs_host);
		row->setObjectName(QStringLiteral("Transparent"));
		auto* layout = new QHBoxLayout(row);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(8);

		auto* icon = new QLabel(row);
		icon->setPixmap(LauncherUi::TintedIcon(QStringLiteral(":/icons/folder.svg"),
		                                       QColor(QStringLiteral("#5F6A80")), QSize(14, 14))
		                    .pixmap(14, 14));
		layout->addWidget(icon);

		auto* path = new QLabel(row);
		path->setObjectName(QStringLiteral("SettingsLabel"));
		path->setText(LauncherUi::ElideText(path->font(), dir, 360));
		path->setToolTip(dir);
		layout->addWidget(path, 1);

		auto* count_label = new QLabel(row);
		count_label->setObjectName(QStringLiteral("SettingsValue"));
		count_label->setText(QStringLiteral("%1 个游戏").arg(count));
		layout->addWidget(count_label);

		auto* remove = new QPushButton(QStringLiteral("移除"), row);
		remove->setObjectName(QStringLiteral("LinkButton"));
		remove->setFlat(true);
		remove->setFixedHeight(20);
		remove->setCursor(Qt::PointingHandCursor);
		connect(remove, &QPushButton::clicked, this,
		        [this, dir]() { m_controller->RemoveGameDirectory(dir); });
		layout->addWidget(remove);
		m_dirs_layout->addWidget(row);
	}
}

void SettingsPage::SetVersion(const QString& version, const QString& detail) {
	m_version        = version;
	m_version_detail = detail;
	if (m_version_value != nullptr) {
		m_version_value->setText(QStringLiteral("v%1").arg(version));
		m_version_value->setToolTip(detail);
	}
}

void SettingsPage::SetEmulatorPath(const QString& path) {
	if (m_emulator_path != nullptr) {
		m_emulator_path->setText(LauncherUi::ElideText(
		    m_emulator_path->font(), path, 330));
		m_emulator_path->setToolTip(path);
	}
}

void SettingsPage::SetGridColumns(int columns) {
	if (m_grid_slider == nullptr) {
		return;
	}
	columns = qBound(0, columns, 5);
	const QSignalBlocker blocker(m_grid_slider);
	m_grid_slider->setValue(columns);
	if (m_grid_value != nullptr) {
		m_grid_value->setText(GridColumnsText(columns));
	}
}

void SettingsPage::SetRuntimeInfo(const QString& device, const QString& api) {
	m_graphics_device = device;
	m_graphics_api    = api;
	if (m_backend_value != nullptr) {
		m_backend_value->setText(api.isEmpty() ? QStringLiteral("自动") : api);
	}
}

void SettingsPage::SetControllerName(const QString& name) {
	m_controller_name = name;
	if (m_pad_value != nullptr) {
		m_pad_value->setText(name.isEmpty() ? QStringLiteral("自动检测") : name);
	}
}

void SettingsPage::SetCheckOnStartup(bool checked) {
	if (m_check_on_startup != nullptr) {
		m_check_on_startup->SetChecked(checked);
	}
}

void SettingsPage::SetCheckUpdatesSupported(bool supported) {
	m_check_updates_supported = supported;
	Refresh();
}

void SettingsPage::SetCheckingForUpdates(bool checking) {
	m_checking = checking;
	Refresh();
}

void SettingsPage::SetInstalling(bool installing) {
	m_installing = installing;
	Refresh();
}

void SettingsPage::SetSection(int section) {
	if (m_left_column == nullptr || m_right_column == nullptr) {
		return;
	}

	const auto clear_column = [](QVBoxLayout* column) {
		while (auto* item = column->takeAt(0)) {
			if (auto* widget = item->widget(); widget != nullptr) {
				widget->hide();
			}
			delete item;
		}
	};
	clear_column(m_left_column);
	clear_column(m_right_column);

	const auto add = [](QVBoxLayout* column, QWidget* card) {
		if (card != nullptr) {
			card->show();
			column->addWidget(card);
		}
	};

	switch (section) {
		case 0:
			add(m_left_column, m_appearance_card);
			add(m_left_column, m_storage_card);
			add(m_left_column, m_audio_card);
			add(m_right_column, m_graphics_card);
			add(m_right_column, m_compatibility_card);
			add(m_right_column, m_update_card);
			add(m_right_column, m_controller_card);
			break;
		case 1: add(m_left_column, m_graphics_card); break;
		case 2: add(m_left_column, m_audio_card); break;
		case 3: add(m_left_column, m_controller_card); break;
		case 4: add(m_left_column, m_storage_card); break;
		case 5: add(m_left_column, m_update_card); break;
		case 6: add(m_left_column, m_compatibility_card); break;
		default: break;
	}
	m_left_column->addStretch(1);
	m_right_column->addStretch(1);

	m_tab_general->setChecked(section == 0);
	m_tab_graphics->setChecked(section == 1);
	m_tab_audio->setChecked(section == 2);
	m_tab_controller->setChecked(section == 3);
	m_tab_storage->setChecked(section == 4);
	m_tab_update->setChecked(section == 5);
	m_tab_compatibility->setChecked(section == 6);

	if (m_scroll != nullptr) {
		m_scroll->verticalScrollBar()->setValue(0);
	}
}
