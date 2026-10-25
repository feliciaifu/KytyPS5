#ifndef LIBRARY_PAGE_H
#define LIBRARY_PAGE_H

#include "configuration.h"

#include <QDateTime>
#include <QSet>
#include <QString>
#include <QVector>
#include <QWidget>

class ConfigurationItem;
class ConfigurationListWidget;
class QEvent;
class QGridLayout;
class QVBoxLayout;
class CoverLoader;
class CoverView;
class ElidedLabel;
class GameCard;
class HeroBanner;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTableWidget;
class QTimer;
class QToolButton;
class StatusPill;

class LibraryPage: public QWidget {
	Q_OBJECT

public:
	enum class Mode { Library, Favorites, Compatibility };
	enum class SortKey { Name, Serial, Size, Status, GameVersion, FirmwareVersion, Path, Comment };

	explicit LibraryPage(ConfigurationListWidget* controller, CoverLoader* covers,
	                     QWidget* parent = nullptr);

	void SetMode(Mode mode);
	void SetFavorites(const QSet<QString>& favorites);
	void SetLastPlayed(const QString& path, const QDateTime& time);
	void SetRunEnabled(bool enabled);
	void SetDirectoryFilter(const QString& directory);
	void SetListMode(bool list_mode);
	void SetGridColumns(int columns);

	[[nodiscard]] Mode GetMode() const { return m_mode; }
	[[nodiscard]] bool IsListMode() const { return m_list_mode; }
	[[nodiscard]] int GetGridColumns() const { return m_grid_columns; }

	void RefreshItems();
	void ResetUiState();

	bool eventFilter(QObject* watched, QEvent* event) override;

signals:
	void FavoritesChanged(const QSet<QString>& favorites);
	void RequestLibraryMode();
	void ViewModeChanged(bool list_mode);

private:
	struct FilterChip {
		QPushButton* button = nullptr;
		int          status = -1;
	};

	void BuildUi();
	void BuildHeader();
	void BuildFilterBar();
	void BuildGridPage();
	void BuildListPage();
	void BuildDetailsPanel();
	void BuildEmptyPage();

	void RefreshItemData();
	void RefreshSelection(ConfigurationItem* item);
	void ClearForRescan();
	void RebuildViews();
	void RebuildCards(const QVector<ConfigurationItem*>& items);
	void RebuildTable(const QVector<ConfigurationItem*>& items);
	void RelayoutCards();
	void UpdateHeader();
	void UpdateChips();
	void UpdateHero();
	void UpdateDetails();
	void UpdateEmptyState();
	void UpdateViewButtons();

	[[nodiscard]] QVector<ConfigurationItem*> VisibleItems() const;
	[[nodiscard]] QVector<ConfigurationItem*> BaseItems() const;
	[[nodiscard]] bool Matches(ConfigurationItem* item) const;
	[[nodiscard]] bool MatchesMode(ConfigurationItem* item) const;
	[[nodiscard]] bool MatchesDirectory(ConfigurationItem* item) const;
	[[nodiscard]] QString FavoriteKey(ConfigurationItem* item) const;
	[[nodiscard]] ConfigurationItem* FindLastPlayed() const;
	[[nodiscard]] ConfigurationItem* HeroItem() const;
	[[nodiscard]] QString LastPlayedText(ConfigurationItem* item) const;
	[[nodiscard]] int RowForItem(ConfigurationItem* item) const;
	[[nodiscard]] ConfigurationItem* ItemForRow(int row) const;

	void SelectItem(ConfigurationItem* item, bool scroll_table = false);
	void PlayItem(ConfigurationItem* item);
	void ToggleFavorite(ConfigurationItem* item);
	void ShowGameMenu(ConfigurationItem* item, const QPoint& global_pos);
	void ShowCompatibilityEditor();
	void ShowHeaderMenu();
	void ShowSortMenu();
	void SortItems(QVector<ConfigurationItem*>& items);
	void UpdateSortButton();
	void Rescan();
	void AddGameDirectory();
	void UpdateTableRow(int row, ConfigurationItem* item);
	void ScheduleCoverLoads();

	void OnCardClicked(GameCard* card);
	void OnCardDoubleClicked(GameCard* card);
	void OnCardContextMenu(GameCard* card, const QPoint& global_pos);
	void OnTableCurrentChanged(int row, int column);

	ConfigurationListWidget* m_controller = nullptr;
	CoverLoader*             m_covers = nullptr;

	Mode      m_mode = Mode::Library;
	QSet<QString> m_favorites;
	QString   m_last_played_path;
	QDateTime m_last_played_time;
	QString   m_directory_filter;
	bool      m_run_enabled = true;
	bool      m_list_mode = false;
	int       m_grid_columns = 0;
	bool      m_syncing = false;
	int       m_status_filter = -1;
	SortKey   m_sort_key = SortKey::Name;
	bool      m_sort_ascending = true;
	QDateTime m_last_scan;

	QVector<ConfigurationItem*> m_items;
	QVector<ConfigurationItem*> m_visible_items;
	QVector<GameCard*>          m_cards;
	QVector<FilterChip>         m_chips;

	QLabel*      m_title = nullptr;
	QLabel*      m_subtitle = nullptr;
	QPushButton* m_sort_button = nullptr;
	QLineEdit*   m_search = nullptr;
	QTimer*      m_search_timer = nullptr;
	QWidget*     m_view_toggle = nullptr;
	QToolButton* m_grid_button = nullptr;
	QToolButton* m_list_button = nullptr;
	QPushButton* m_more_button = nullptr;

	QVBoxLayout*    m_root = nullptr;
	QStackedWidget* m_content = nullptr;
	QWidget*        m_grid_page = nullptr;
	QWidget*        m_list_page = nullptr;
	QWidget*        m_empty_page = nullptr;
	HeroBanner*     m_hero = nullptr;
	QScrollArea*    m_scroll = nullptr;
	QWidget*        m_cards_host = nullptr;
	QGridLayout*    m_cards_layout = nullptr;
	QLabel*         m_empty_icon = nullptr;
	QLabel*         m_empty_title = nullptr;
	QLabel*         m_empty_text = nullptr;
	QPushButton*    m_empty_button = nullptr;

	QTableWidget* m_table = nullptr;
	QWidget*      m_details = nullptr;
	QStackedWidget* m_details_stack = nullptr;
	CoverView*    m_detail_cover = nullptr;
	ElidedLabel*  m_detail_title = nullptr;
	QLabel*       m_detail_meta = nullptr;
	StatusPill*   m_detail_status = nullptr;
	QLabel*       m_detail_community = nullptr;
	QLabel*       m_detail_firmware = nullptr;
	QLabel*       m_detail_size = nullptr;
	QLabel*       m_detail_path = nullptr;
	QLabel*       m_detail_last_run = nullptr;
	QPushButton*  m_detail_play = nullptr;
	QPushButton*  m_detail_settings = nullptr;
	QPushButton*  m_detail_input = nullptr;
	QToolButton*  m_detail_favorite = nullptr;
	QToolButton*  m_detail_more = nullptr;
	QPushButton*  m_detail_compat_edit = nullptr;
	QLabel*       m_detail_trophy_value = nullptr;
	QLabel*       m_detail_trophy_note = nullptr;
	QPushButton*  m_detail_trophy = nullptr;
	QLabel*       m_detail_note = nullptr;
	QWidget*      m_detail_empty = nullptr;
	QLabel*       m_table_footer = nullptr;

	ConfigurationItem* m_selected = nullptr;
};

#endif // LIBRARY_PAGE_H
