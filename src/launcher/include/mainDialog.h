#ifndef MAIN_DIALOG_H
#define MAIN_DIALOG_H

#include <QByteArray>
#include <QDialog>
#include <QString>

class QWidget;
class QProcess;
class MainDialogPrivate;
class QSettings;
class QResizeEvent;

class Configuration;
class MainDialog: public QDialog {
	Q_OBJECT

signals:
	void Start();
	void Resize();

public:
	explicit MainDialog(QWidget* parent = nullptr);
	~MainDialog() override = default;

	void RunInterpreter(QProcess* process, const Configuration& info, bool close_terminal);

	static void WriteSettings(QSettings& s);
	static void ReadSettings(QSettings& s);

	void resizeEvent(QResizeEvent* event) override;

#if defined(_WIN32)
	bool nativeEvent(const QByteArray& event_type, void* message, qintptr* result) override;
#endif

private:
	MainDialogPrivate* m_p = nullptr;
};

#endif // MAIN_DIALOG_H
