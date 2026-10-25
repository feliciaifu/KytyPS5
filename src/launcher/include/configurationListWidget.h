#ifndef CONFIGURATION_LIST_WIDGET_H
#define CONFIGURATION_LIST_WIDGET_H

#include "configuration.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <memory>

class ConfigurationItem;
class CompatibilityDatabase;
class ControllerPreview;
class QEvent;
class QTreeWidgetItem;
class QTreeWidget;

namespace Ui {
class ConfigurationListWidget;
} // namespace Ui

class ConfigurationListWidget: public QWidget {
	Q_OBJECT

public:
	explicit ConfigurationListWidget(QWidget* parent = nullptr);
	~ConfigurationListWidget() override;

	void SetRunEnabled(bool flag) { m_run_enabled = flag; }
	void SetRuntimeDirectory(const QString& directory) { m_runtime_directory = directory; }
	void SetControllerPreview(ControllerPreview* preview) { m_controller_preview = preview; }

	[[nodiscard]] const ConfigurationItem* GetSelectedItem() const { return m_selected_item; }
	ConfigurationItem*                     GetSelectedItem() { return m_selected_item; }

	[[nodiscard]] const QString& GetSettingsFile() const { return m_settings_file; }
	[[nodiscard]] const QString& GetGlobalControllerColor() const {
		return m_global_info.controller.color;
	}

	[[nodiscard]] QTreeWidget*             GetTreeWidget() const;
	[[nodiscard]] QList<ConfigurationItem*> GetItems() const;
	[[nodiscard]] const Configuration&     GetGlobalInfo() const { return m_global_info; }
	Configuration&                         GetGlobalInfo() { return m_global_info; }
	[[nodiscard]] const QStringList&       GetGameDirectories() const { return m_game_dirs; }
	[[nodiscard]] const QString&           GetAddonsDirectory() const { return m_addons_dir; }
	[[nodiscard]] const QString&           GetRuntimeDirectory() const { return m_runtime_directory; }

	void SelectItemByPointer(ConfigurationItem* item);
	void RequestRun();
	void EditSelectedGame();
	void DeleteSelectedGame();
	void OpenSelectedGameFolder();
	void OpenSelectedSaveDataFolder();
	void RemoveSelectedSaveData();
	void CompressSelectedGame();
	void ShowPatchesForSelected();
	void ShowAdditionalContentForSelected();
	void EditGlobalSettings();
	void EditInputMapping();
	void AddGameDirectoryInteractive(QWidget* parent);
	void AddGameDirectory(const QString& dir);
	void RemoveGameDirectory(const QString& dir);
	[[nodiscard]] bool CanEditCompatibility(const ConfigurationItem& item) const;
	void SetCompatibilityStatus(const QString& title_id, Configuration::GameStatus status);
	void SetCompatibilityComment(const QString& title_id, const QString& comment);
	void OpenSettingsFileFolder();
	[[nodiscard]] std::unique_ptr<Configuration>
	CreateConfiguration(const ConfigurationItem& item) const;

	bool EnsureGameDirectory();
	void ScanGameDirectory();
	void ViewTrophies();
	void ViewTrophyOverview();

signals:

	void Run();
	void Select();
	void PreviewControllerColor(const QString& color);
	void ItemsChanged();
	void SelectionChanged(ConfigurationItem* item);

protected:
	void changeEvent(QEvent* event) override;

public slots:
	void WriteSettings();
	void ReadSettings();

protected slots:

	void edit_configuration();
	void delete_configuartion();
	void edit_global_settings();
	void edit_input_mapping();
	void list_itemDoubleClicked(QTreeWidgetItem* witem, int column);
	void open_game_folder();
	void remove_save_data();
	void compress_game();
	void filter_configurations(const QString& text);

private:
	void               SelectItem(QTreeWidgetItem* witem);
	void               ShowContextMenu(QTreeWidgetItem* witem);
	void               ApplyCompatibility();
	void               UpdateToolbarIcons();
	void               ImportGameSettings(QWidget* parent);
	void               ExportGameSettings(QWidget* parent) const;
	[[nodiscard]] bool HasValidGameDirectory() const;

	ConfigurationItem*            m_selected_item = nullptr;
	bool                          m_run_enabled   = true;
	Ui::ConfigurationListWidget*  m_ui            = nullptr;
	QString                       m_settings_file;
	QString                       m_runtime_directory;
	QStringList                   m_game_dirs;
	QString                       m_addons_dir;
	Configuration                 m_global_info;
	QMap<QString, Configuration*> m_custom_infos;
	CompatibilityDatabase*        m_compatibility = nullptr;
	ControllerPreview*            m_controller_preview = nullptr;
};

#endif // CONFIGURATION_LIST_WIDGET_H
