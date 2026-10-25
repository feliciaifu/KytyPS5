#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <QWidget>

class ConfigurationListWidget;
class QComboBox;
class QLabel;
class QLayout;
class QPushButton;
class QScrollArea;
class QSlider;
class QTimer;
class QVBoxLayout;
class ToggleSwitch;

class SettingsPage: public QWidget {
	Q_OBJECT

public:
	explicit SettingsPage(ConfigurationListWidget* controller, QWidget* parent = nullptr);

	void Refresh();
	void SetVersion(const QString& version, const QString& detail = {});
	void SetRuntimeInfo(const QString& device, const QString& api);
	void SetEmulatorPath(const QString& path);
	void SetGridColumns(int columns);
	void SetControllerName(const QString& name);
	void SetCheckOnStartup(bool checked);
	void SetCheckUpdatesSupported(bool supported);
	void SetCheckingForUpdates(bool checking);
	void SetInstalling(bool installing);

signals:
	void CheckForUpdatesRequested();
	void CheckOnStartupChanged(bool checked);
	void GridColumnsChanged(int columns);
	void ResetRequested();
	void OpenGlobalSettingsRequested();

private:
	void BuildUi();
	QWidget* AddCard(QVBoxLayout* column, const QString& title, QVBoxLayout** body);
	void AddRow(QVBoxLayout* body, const QString& label, QWidget* value);
	QLabel* AddTextRow(QVBoxLayout* body, const QString& label);
	void AddLinkRow(QVBoxLayout* body, const QString& label, QPushButton** button);
	void SetSection(int section);
	void UpdatePaths();
	void UpdateSummary();


	ConfigurationListWidget* m_controller = nullptr;

	QString m_version;
	QString m_version_detail;
	QString m_graphics_device;
	QString m_graphics_api;
	QString m_controller_name;
	bool    m_check_updates_supported = true;
	bool    m_checking = false;
	bool    m_installing = false;
	bool    m_updating = false;

	QScrollArea*    m_scroll = nullptr;
	QWidget*        m_scroll_content = nullptr;
	QWidget*        m_appearance_card = nullptr;
	QWidget*        m_graphics_card = nullptr;
	QWidget*        m_storage_card = nullptr;
	QWidget*        m_update_card = nullptr;
	QWidget*        m_audio_card = nullptr;
	QWidget*        m_controller_card = nullptr;
	QWidget*        m_compatibility_card = nullptr;

	QPushButton* m_tab_general = nullptr;
	QPushButton* m_tab_graphics = nullptr;
	QPushButton* m_tab_audio = nullptr;
	QPushButton* m_tab_controller = nullptr;
	QPushButton* m_tab_storage = nullptr;
	QPushButton* m_tab_update = nullptr;

	QLabel* m_theme_value = nullptr;
	QLabel* m_language_value = nullptr;
	QLabel* m_scale_value = nullptr;
	QLabel* m_backend_value = nullptr;
	QComboBox* m_device_combo = nullptr;
	QComboBox* m_resolution_combo = nullptr;
	QLabel* m_audio_backend_value = nullptr;
	QComboBox* m_audio_output_combo = nullptr;
	QLabel*  m_volume_value = nullptr;
	QSlider* m_volume_slider = nullptr;
	QLabel*  m_speaker_volume_value = nullptr;
	QSlider* m_speaker_volume_slider = nullptr;
	QLabel*  m_grid_value = nullptr;
	QSlider* m_grid_slider = nullptr;
	QTimer*  m_save_timer = nullptr;
	QLabel* m_pad_value = nullptr;
	QLabel* m_mapping_value = nullptr;
	QLabel* m_trigger_value = nullptr;
	QLabel* m_version_value = nullptr;
	QLabel* m_emulator_path = nullptr;
	QLabel* m_update_status = nullptr;
	QLabel* m_config_path = nullptr;

	QLayout* m_dirs_layout = nullptr;
	QVBoxLayout* m_left_column = nullptr;
	QVBoxLayout* m_right_column = nullptr;
	QWidget* m_dirs_host = nullptr;

	ToggleSwitch* m_check_on_startup = nullptr;
	ToggleSwitch* m_red_zone_toggle = nullptr;
	QPushButton*  m_check_updates = nullptr;
};

#endif // SETTINGS_PAGE_H
