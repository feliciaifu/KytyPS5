#include "mainDialog.h"

#include "cheatFile.h"
#include "configuration.h"
#include "configurationItem.h"
#include "configurationListWidget.h"
#include "controllerPreview.h"
#include "gameContent.h"
#include "launcherShell.h"
#include "titleSound.h"
#include "updateChecker.h"

#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QLabel>
#include <QMessageBox>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>
#include <QStyleHints>
#include <QTextStream>
#include <QVariant>
#include <QVBoxLayout>
#include <QWindow>
#include <QtCore>

#include <cstdint>

#if defined(_WIN32)
#include <windows.h> // IWYU pragma: keep
#include <windowsx.h> // IWYU pragma: keep
#endif

// IWYU pragma: no_include <minwindef.h>
// IWYU pragma: no_include <processthreadsapi.h>
// IWYU pragma: no_include <winbase.h>

class QWidget;

#if defined(_WIN32)
constexpr char EMULATOR_EXE[] = "kyty_emulator.exe";
#else
constexpr char EMULATOR_EXE[] = "kyty_emulator";
#endif

#if defined(_WIN32)
constexpr char CMD_EXE[] = "cmd.exe";
#elif defined(__linux__)
constexpr char KYTY_BASH_FILE[] = "kyty_run.sh";
#endif
#if defined(_WIN32)
constexpr DWORD CMD_X_CHARS = 175;
constexpr DWORD CMD_Y_CHARS = 1000;
#endif
constexpr char SETTINGS_MAIN_DIALOG[]        = "MainDialog";
constexpr char SETTINGS_MAIN_LAST_GEOMETRY[] = "geometry";
constexpr char SETTINGS_CHECK_UPDATES[]      = "check_updates_on_startup";

class MainDialogPrivate: public QObject {
	Q_OBJECT

public:
	explicit MainDialogPrivate(QObject* parent = nullptr): QObject(parent) {}
	~MainDialogPrivate() override;

	void Setup(MainDialog* main_dialog);

	/*slots:*/

	void Update();
	void FindInterpreter();
	void Run();

	[[nodiscard]] const QString& GetInterpreter() const { return m_interpreter; }

	static void WriteSettings(QSettings& s);
	static void ReadSettings(QSettings& s);

private:
	static QByteArray g_last_geometry;
	static bool       g_check_updates_on_startup;

	ConfigurationListWidget* m_controller = nullptr;
	LauncherShell*           m_shell = nullptr;
	MainDialog*              m_main_dialog = nullptr;
	UpdateChecker*           m_update_checker = nullptr;
	ControllerPreview        m_controller_preview;
	TitleSound               m_title_sound;
	QString                  m_interpreter;

	QProcess m_process;

	QPointer<ConfigurationItem> m_running_item;
};

QByteArray MainDialogPrivate::g_last_geometry;
bool       MainDialogPrivate::g_check_updates_on_startup = true;

MainDialog::MainDialog(QWidget* parent): QDialog(parent), m_p(new MainDialogPrivate(this)) {
	m_p->Setup(this);
}

MainDialogPrivate::~MainDialogPrivate() {
	m_process.disconnect(this);
}

void MainDialogPrivate::Setup(MainDialog* main_dialog) {
	m_main_dialog = main_dialog;

	m_controller = new ConfigurationListWidget(main_dialog);
	m_controller->hide();
	m_controller->SetControllerPreview(&m_controller_preview);

	m_shell = new LauncherShell(m_controller, main_dialog);
	auto* layout = new QVBoxLayout(main_dialog);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(m_shell);

	main_dialog->setWindowTitle(QStringLiteral("KytyPS5 Launcher"));
	main_dialog->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog |
	                            Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint |
	                            Qt::WindowCloseButtonHint);
	main_dialog->setMinimumSize(1180, 720);
	if (g_last_geometry.isEmpty()) {
		main_dialog->resize(1440, 900);
	}

	m_update_checker = new UpdateChecker(main_dialog);
	m_shell->SetCheckOnStartup(g_check_updates_on_startup);
	m_shell->SetCheckUpdatesSupported(UpdateChecker::IsSupported());

	connect(main_dialog, &MainDialog::Start, this, &MainDialogPrivate::FindInterpreter,
	        Qt::QueuedConnection);
	connect(m_controller, &ConfigurationListWidget::Select, this, [this]() {
		Update();
		const auto* item = m_controller->GetSelectedItem();
		m_title_sound.SetTitle(m_process.state() == QProcess::NotRunning && item != nullptr
		                           ? item->GetInfo().basedir
		                           : QString());
	});
	connect(m_controller, &ConfigurationListWidget::PreviewControllerColor, this,
	        [this](const QString& color) {
		        if (m_process.state() == QProcess::NotRunning) {
			        m_controller_preview.SetColor(color);
		        }
	        });
	connect(m_controller, &ConfigurationListWidget::Run, this, &MainDialogPrivate::Run);
	connect(m_shell, &LauncherShell::CheckForUpdatesRequested, this,
	        [this]() { m_update_checker->Check(true); });
	connect(m_shell, &LauncherShell::CheckOnStartupChanged, this, [this](bool checked) {
		g_check_updates_on_startup = checked;
		m_controller->WriteSettings();
	});
	connect(m_update_checker, &UpdateChecker::CheckingChanged, m_shell,
	        &LauncherShell::SetCheckingForUpdates);
	connect(m_update_checker, &UpdateChecker::InstallingChanged, m_shell,
	        &LauncherShell::SetInstalling);
	connect(m_update_checker, &UpdateChecker::InstallingChanged, this, &MainDialogPrivate::Update);
	connect(&m_controller_preview, &ControllerPreview::StateChanged, this, [this]() {
		m_shell->SetControllerName(m_controller_preview.DeviceName());
	});
	connect(main_dialog, &MainDialog::Resize, [this]() {
		g_last_geometry = m_main_dialog->saveGeometry();
		m_controller->WriteSettings();
	});

	connect(&m_process, &QProcess::stateChanged, this, [this](QProcess::ProcessState state) {
		m_update_checker->SetGameRunning(state != QProcess::NotRunning);
		if (state == QProcess::NotRunning) {
			if (m_running_item != nullptr) {
				m_running_item->SetRunning(false);
			}
			Update();
		}
	});

	m_main_dialog->restoreGeometry(g_last_geometry);
	if (m_main_dialog->width() < 1280 || m_main_dialog->height() < 800) {
		m_main_dialog->resize(1440, 900);
	}

	Update();
}

void MainDialogPrivate::FindInterpreter() {
	UpdateChecker::ShowPreviousResult(m_main_dialog);
	QDir search_dir(QApplication::applicationDirPath());
	m_interpreter = search_dir.absoluteFilePath(EMULATOR_EXE);

	if (!QFile::exists(m_interpreter)) {
		search_dir.cdUp();
		m_interpreter = search_dir.absoluteFilePath(EMULATOR_EXE);
	}

	bool found = QFile::exists(m_interpreter);
	m_controller->SetRuntimeDirectory(QFileInfo(m_interpreter).absolutePath());
	m_shell->SetEmulatorPath(m_interpreter);

	if (found) {
		QProcess test;
		test.setProgram(m_interpreter);
		test.start();
		test.waitForFinished();

		auto output = QString(test.readAllStandardOutput());
		auto lines  = output.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);

		if (lines.count() >= 2) {
			const auto version_line =
			    lines.at(0).startsWith("exe_name") ? lines.at(1) : lines.at(0);
			const auto match =
			    QRegularExpression(QStringLiteral("ver\\s*=\\s*([0-9.]+)")).match(version_line);
			const auto version = match.hasMatch() ? match.captured(1) : QStringLiteral("0.0.0");
			m_shell->SetEmulatorVersion(version, version_line);
		} else {
			found = false;
		}
	}

	if (!found) {
		QMessageBox::critical(m_main_dialog, tr("Error"), tr("Can't find emulator"));
		QApplication::quit();
		return;
	}

	Update();
	if (g_check_updates_on_startup) {
		m_update_checker->Check(false);
	}
}

static void AddFlag(QStringList& args, bool enabled, const char* name) {
	if (enabled) {
		args << name;
	}
}

static QStringList CreateEmulatorArgs(const Configuration& info) {
	QStringList args;
	auto        r = EnumToText(info.screen_resolution).split('x');

	if (r.size() != 2) {
		return {};
	}

	args << "--screen-width" << r.at(0);
	args << "--screen-height" << r.at(1);
	args << "--user-name" << info.user_name;
	args << "--user-id" << QString::number(info.user_id);
	if (!info.audio_input_device.isEmpty()) {
		args << "--mic" << info.audio_input_device;
	}
	if (!info.audio_output_device.isEmpty()) {
		args << "--audio-device" << info.audio_output_device;
	}
	args << "--audio-volume" << QString::number(info.audio_volume);
	if (!info.controller.color.isEmpty()) {
		args << "--controller-color" << info.controller.color;
	}
	args << "--controller-volume" << QString::number(info.controller.speaker_volume);
	args << "--controller-vibration" << QString::number(info.controller.vibration_intensity);
	args << "--keyboard-player" << QString::number(info.controller.keyboard_player);
	args << "--present-mode" << EnumToText(info.present_mode);
	if (info.gpu_index >= 0) {
		args << "--gpu" << QString::number(info.gpu_index);
	}
	AddFlag(args, info.fullscreen_enabled, "--fullscreen");
	AddFlag(args, info.hdr_enabled, "--hdr");
	AddFlag(args, info.hide_cursor_enabled, "--hide-cursor");
	AddFlag(args, info.readback_linear_images, "--readback-linear-images");
	AddFlag(args, info.readback_tiled_images, "--readback-tiled-images");
	AddFlag(args, info.sync_raw_image_buffers, "--sync-raw-image-buffers");
	AddFlag(args, !info.trophy_enabled, "--no-trophy-notifications");
	args << "--trophy-duration" << QString::number(info.trophy_duration_seconds);
	AddFlag(args, info.perf_overlay_enabled, "--perf-overlay");
	AddFlag(args, info.skip_notice_screen, "--skip-notice-screen");
	AddFlag(args, info.network_connected, "--network-connected");
	AddFlag(args, info.tessellation_enabled, "--tessellation");
	AddFlag(args, info.occlusion_queries_enabled, "--occlusion-queries");
	args << "--vblank-frequency" << QString::number(info.vblank_frequency);
	args << "--console-language" << QString::number(info.console_language);
	AddFlag(args, info.vulkan_validation_enabled, "--vulkan-validation");
	AddFlag(args, info.shader_validation_enabled, "--shader-validation");
	args << "--shader-optimization-type" << EnumToText(info.shader_optimization_type);
	args << "--shader-log-direction" << EnumToText(info.shader_log_direction);
	args << "--shader-log-folder" << info.shader_log_folder;
	AddFlag(args, info.command_buffer_dump_enabled, "--command-buffer-dump");
	args << "--command-buffer-dump-folder" << info.command_buffer_dump_folder;
	args << "--printf-direction" << EnumToText(info.printf_direction);
	args << "--printf-output-file" << info.printf_output_file;
	AddFlag(args, info.profiler_enabled, "--profile");
	AddFlag(args, info.amd_cpu_enabled, "--amd-cpu");
#if defined(_WIN32)
	AddFlag(args, info.red_zone_protection_enabled, "--redzone");
#endif
	for (const auto& binding: info.host_input_mapping) {
		args << "--keymap" << binding;
	}
	AddFlag(args, info.renderdoc_enabled, "--rd");

	QString game = info.basedir;
	if (!info.elf.isEmpty() && !GameContent::IsArchive(info.basedir)) {
		game = QDir(info.basedir).filePath(info.elf);
	}
	args << "--game" << game;
	if (!info.addons_dir.isEmpty()) {
		args << "--addcont-dir" << info.addons_dir;
	}

	const auto patch_plan = Cheats::PlanPath(info.title_id);
	if (QFileInfo::exists(patch_plan)) {
		args << "--game-patch" << patch_plan;
	}

	return args;
}

#ifdef __linux__
static QString BashQuote(QString value) {
	value.replace('\'', "'\\''");
	return QStringLiteral("'") + value + QStringLiteral("'");
}

static bool CreateBashScript(const QString& interpreter, const QStringList& args,
                             const QString& file_name) {
	QFile file(file_name);
	if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QTextStream s(&file);

		s << "#!/bin/bash\n";
		s << BashQuote(interpreter);
		for (const auto& arg: args) {
			s << " " << BashQuote(arg);
		}
		s << "\n";
		s << "echo Press any key...\n";
		s << "read -n1\n";

		file.close();

		return file.setPermissions(file.permissions() | QFile::ExeUser | QFile::ExeOwner |
		                           QFile::ExeGroup);
	}
	return false;
}

// Find a terminal and its command separator.
static bool FindTerminal(QString* program, QStringList* prefix) {
	struct TerminalSpec {
		const char* executable;
		const char* separator; // nullptr when the command follows immediately
	};

	static const TerminalSpec candidates[] = {
	    {"x-terminal-emulator", "-e"},
	    {"gnome-terminal", "--"},
	    {"konsole", "-e"},
	    {"xfce4-terminal", "-x"},
	    {"mate-terminal", "--"},
	    {"tilix", "-e"},
	    {"alacritty", "-e"},
	    {"kitty", nullptr},
	    {"foot", nullptr},
	    {"wezterm", "-e"},
	    {"urxvt", "-e"},
	    {"xterm", "-e"},
	};

	const auto try_candidate = [program, prefix](const QString& executable, const char* separator) {
		const auto resolved = QStandardPaths::findExecutable(executable);
		if (resolved.isEmpty()) {
			return false;
		}
		*program = resolved;
		prefix->clear();
		if (separator != nullptr) {
			*prefix << QString::fromLatin1(separator);
		}
		return true;
	};

	if (const auto from_env = qEnvironmentVariable("TERMINAL"); !from_env.isEmpty()) {
		// Reuse the known separator for an explicit terminal.
		const auto  env_name  = QFileInfo(from_env).fileName();
		const char* separator = "-e";
		for (const auto& candidate: candidates) {
			if (env_name == QLatin1String(candidate.executable)) {
				separator = candidate.separator;
				break;
			}
		}
		if (try_candidate(from_env, separator)) {
			return true;
		}
	}

	for (const auto& candidate: candidates) {
		if (try_candidate(QString::fromLatin1(candidate.executable), candidate.separator)) {
			return true;
		}
	}

	return false;
}
#endif

#if defined(_WIN32)
// Quote one token for cmd.exe so paths with spaces survive /K parsing.
static QString WinCmdQuote(QString value) {
	value.replace(QLatin1Char('"'), QStringLiteral("\\\""));
	return QLatin1Char('"') + value + QLatin1Char('"');
}

static QString BuildWinCmdKCommand(const QString& interpreter, const QStringList& args) {
	QString command = WinCmdQuote(QDir::toNativeSeparators(interpreter));
	for (const auto& arg: args) {
		command += QLatin1Char(' ');
		command += WinCmdQuote(arg);
	}
	return command;
}
#endif

void MainDialog::RunInterpreter(QProcess* process, const Configuration& info) {
	const auto& interpreter = m_p->GetInterpreter();

	QFileInfo f(interpreter);
	auto      dir = f.absoluteDir();

	auto args = CreateEmulatorArgs(info);
	if (args.isEmpty()) {
		QMessageBox::critical(this, tr("Error"), tr("Invalid emulator configuration"));
		QApplication::quit();
		return;
	}

#ifdef __linux__
	auto bash_file_name = dir.filePath(KYTY_BASH_FILE);
	if (!CreateBashScript(interpreter, args, bash_file_name)) {
		QMessageBox::critical(this, tr("Error"), tr("Can't create file:\n") + bash_file_name);
		QApplication::quit();
		return;
	}

	{
		QString     terminal;
		QStringList terminal_prefix;
		// Pass the script as a file argument (not bash -c) so paths with spaces work.
		if (FindTerminal(&terminal, &terminal_prefix)) {
			process->setProgram(terminal);
			process->setArguments(terminal_prefix + QStringList {"bash", bash_file_name});
		} else {
			// Run without a terminal as a fallback.
			process->setProgram(QStringLiteral("bash"));
			process->setArguments({bash_file_name});
		}
	}
#elif defined(_WIN32)
	{
		// Use nativeArguments so Qt does not re-quote the /K command string.
		process->setProgram(CMD_EXE);
		process->setArguments({});
		process->setNativeArguments(QStringLiteral("/K \"") +
		                            BuildWinCmdKCommand(interpreter, args) + QLatin1Char('"'));
	}
#else
	process->setProgram(interpreter);
	process->setArguments(args);
#endif
	process->setWorkingDirectory(dir.path());
#if defined(_WIN32)
	process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
		args->flags |= static_cast<uint32_t>(CREATE_NEW_CONSOLE);
		args->startupInfo->dwFlags &= ~static_cast<DWORD>(STARTF_USESTDHANDLES);
		args->startupInfo->dwFlags |= static_cast<DWORD>(STARTF_USECOUNTCHARS);
		args->startupInfo->dwXCountChars = CMD_X_CHARS;
		args->startupInfo->dwYCountChars = CMD_Y_CHARS;
		// args->startupInfo->dwFlags |= static_cast<DWORD>(STARTF_USEFILLATTRIBUTE);
		// args->startupInfo->dwFillAttribute =
		//     static_cast<DWORD>(BACKGROUND_BLUE) | static_cast<DWORD>(FOREGROUND_RED) |
		//     static_cast<DWORD>(FOREGROUND_INTENSITY);
	});
#endif
	process->start();
#if !defined(_WIN32)
	// Report immediate launch failures.
	if (!process->waitForStarted(5000)) {
		QMessageBox::critical(
		    this, tr("Error"),
		    tr("Failed to start:\n%1\n\n%2").arg(process->program(), process->errorString()));
		return;
	}
#endif
	process->waitForFinished(100);
}

void MainDialog::WriteSettings(QSettings& s) {
	MainDialogPrivate::WriteSettings(s);
}

void MainDialog::ReadSettings(QSettings& s) {
	MainDialogPrivate::ReadSettings(s);
}

void MainDialog::resizeEvent(QResizeEvent* event) {
	emit Resize();
	QDialog::resizeEvent(event);
}

void MainDialogPrivate::WriteSettings(QSettings& s) {
	s.beginGroup(SETTINGS_MAIN_DIALOG);

	if (!g_last_geometry.isEmpty()) {
		s.setValue(SETTINGS_MAIN_LAST_GEOMETRY, g_last_geometry);
	}
	s.setValue(SETTINGS_CHECK_UPDATES, g_check_updates_on_startup);

	s.endGroup();
}

void MainDialogPrivate::ReadSettings(QSettings& s) {
	s.beginGroup(SETTINGS_MAIN_DIALOG);

	g_last_geometry = s.value(SETTINGS_MAIN_LAST_GEOMETRY, g_last_geometry).toByteArray();
	g_check_updates_on_startup = s.value(SETTINGS_CHECK_UPDATES, true).toBool();

	s.endGroup();
}

void MainDialogPrivate::Run() {
	if (m_update_checker->IsInstalling()) {
		return;
	}
	m_running_item = m_controller->GetSelectedItem();
	if (m_running_item == nullptr) {
		return;
	}

	m_running_item->SetRunning(true);
	m_controller_preview.SetSuspended(true);
	m_title_sound.SetTitle({});

	auto info = m_controller->CreateConfiguration(*m_running_item);
	m_main_dialog->RunInterpreter(&m_process, *info);

	Update();
}

void MainDialogPrivate::Update() {
	const auto* item = m_controller->GetSelectedItem();

	bool run_enabled = (m_process.state() == QProcess::NotRunning && item != nullptr &&
	                    !m_update_checker->IsInstalling());

	if (run_enabled) {
		const auto& info = item->GetInfo();
		auto        dir  = info.basedir;
		run_enabled      = !dir.isEmpty() && (QDir(dir).exists() || GameContent::IsArchive(dir));
	}

	m_controller->SetRunEnabled(run_enabled);
	m_shell->SetRunEnabled(run_enabled);
	if (m_process.state() != QProcess::NotRunning) {
		m_controller_preview.SetSuspended(true);
		return;
	}
	m_controller_preview.SetSuspended(false);
	m_controller_preview.SetColor(m_controller->GetGlobalControllerColor());
	m_shell->SetControllerName(m_controller_preview.DeviceName());
}

#if defined(_WIN32)
bool MainDialog::nativeEvent(const QByteArray& event_type, void* message, qintptr* result) {
	if (event_type == "windows_generic_MSG" || event_type == "windows_dispatcher_MSG") {
		auto* msg = static_cast<MSG*>(message);
		if (msg->message == WM_NCHITTEST && !isMaximized() && !isFullScreen()) {
			constexpr int border = 7;
			const qreal   dpr    = devicePixelRatioF();
			const QPoint  pos    = mapFromGlobal(
			    QPointF(GET_X_LPARAM(msg->lParam) / dpr, GET_Y_LPARAM(msg->lParam) / dpr).toPoint());
			const bool left   = pos.x() < border;
			const bool right  = pos.x() >= width() - border;
			const bool top    = pos.y() < border;
			const bool bottom = pos.y() >= height() - border;

			if (top && left) {
				*result = HTTOPLEFT;
			} else if (top && right) {
				*result = HTTOPRIGHT;
			} else if (bottom && left) {
				*result = HTBOTTOMLEFT;
			} else if (bottom && right) {
				*result = HTBOTTOMRIGHT;
			} else if (left) {
				*result = HTLEFT;
			} else if (right) {
				*result = HTRIGHT;
			} else if (top) {
				*result = HTTOP;
			} else if (bottom) {
				*result = HTBOTTOM;
			} else {
				return false;
			}
			return true;
		}
	}
	return QDialog::nativeEvent(event_type, message, result);
}
#endif

#include "mainDialog.moc"
