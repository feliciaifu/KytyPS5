#ifndef LAUNCHER_SHELL_H
#define LAUNCHER_SHELL_H

#include <QDateTime>
#include <QSet>
#include <QString>
#include <QWidget>

class ConfigurationItem;
class ConfigurationListWidget;
class CoverLoader;
class LibraryPage;
class SettingsPage;
class SavesPage;
class QLabel;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

class LauncherShell: public QWidget {
	Q_OBJECT

public:
	explicit LauncherShell(ConfigurationListWidget* controller, QWidget* parent = nullptr);

	void SetVersion(const QString& version, const QString& detail = {});
	void SetEmulatorVersion(const QString& version, const QString& detail = {});
	void SetEmulatorPath(const QString& path);
	void SetRuntimeInfo(const QString& device, const QString& api);
	void SetControllerName(const QString& name);
	void SetCheckOnStartup(bool checked);
	void SetCheckUpdatesSupported(bool supported);
	void SetCheckingForUpdates(bool checking);
	void SetInstalling(bool installing);
	void SetRunEnabled(bool enabled);

signals:
	void CheckForUpdatesRequested();
	void CheckOnStartupChanged(bool checked);

protected:
	void showEvent(QShowEvent* event) override;

private:
	enum class Section { Library, Favorites, Saves, Compatibility, Settings };

	void BuildUi();
	QWidget* BuildTitleBar();
	QWidget* BuildSidebar();
	void BuildNavigation(QVBoxLayout* layout);
	void BuildDirectoryList(QVBoxLayout* layout);
	void NavigateTo(Section section);
	void SetDirectoryFilter(const QString& directory);
	void UpdateSidebarIcons();
	void UpdateNavigationChecks();
	void UpdateDirectoryChecks();
	void UpdateRuntimeCard();
	void RefreshSidebar();
	void RefreshDirectoryButtons();
	void RecordLastPlayed();
	void LoadUiSettings();
	void SaveUiSettings();
	void ResetUi();

	ConfigurationListWidget* m_controller = nullptr;
	CoverLoader*             m_covers = nullptr;
	LibraryPage*             m_library = nullptr;
	SettingsPage*            m_settings = nullptr;
	SavesPage*               m_saves = nullptr;
	QStackedWidget*          m_pages = nullptr;

	QWidget*  m_title_bar = nullptr;
	QLabel*   m_version_chip = nullptr;
	QPushButton* m_maximize_button = nullptr;

	QPushButton* m_nav_library = nullptr;
	QPushButton* m_nav_favorites = nullptr;
	QPushButton* m_nav_saves = nullptr;
	QPushButton* m_nav_compatibility = nullptr;
	QPushButton* m_nav_settings = nullptr;
	QPushButton* m_nav_input = nullptr;

	QWidget*     m_dirs_host = nullptr;
	QVBoxLayout* m_dirs_layout = nullptr;

	QLabel* m_runtime_title = nullptr;
	QLabel* m_runtime_meta = nullptr;
	QLabel* m_runtime_detail = nullptr;
	QLabel* m_runtime_dot = nullptr;

	QString    m_runtime_device;
	QString    m_runtime_api;
	QString    m_controller_name;
	QString    m_directory_filter;
	QString    m_last_played_path;
	QDateTime  m_last_played_time;
	QSet<QString> m_favorites;
	Section    m_section = Section::Library;
	bool       m_run_enabled = true;
	bool       m_ui_loaded = false;
	bool       m_restoring_presets = false;
};

#endif // LAUNCHER_SHELL_H
