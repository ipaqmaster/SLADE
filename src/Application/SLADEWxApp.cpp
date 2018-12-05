
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    SLADEWxApp.cpp
// Description: SLADEWxApp class functions.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301, USA.
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
//
// Includes
//
// -----------------------------------------------------------------------------
#include "Main.h"
#include "SLADEWxApp.h"
#include "App.h"
#include "Archive/ArchiveManager.h"
#include "External/email/wxMailer.h"
#include "General/Console/Console.h"
#include "General/Web.h"
#include "MainEditor/MainEditor.h"
#include "MainEditor/UI/ArchiveManagerPanel.h"
#include "MainEditor/UI/MainWindow.h"
#include "MainEditor/UI/StartPage.h"
#include "OpenGL/OpenGL.h"
#include <wx/statbmp.h>

#undef BOOL

#ifdef UPDATEREVISION
#include "gitinfo.h"
#endif


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
namespace Global
{
string error = "";

int    beta_num    = 5;
int    version_num = 3120;
string version     = "3.1.2 Beta 5";
#ifdef GIT_DESCRIPTION
string sc_rev = GIT_DESCRIPTION;
#else
string sc_rev = "";
#endif

#ifdef DEBUG
bool debug = true;
#else
bool   debug  = false;
#endif

int win_version_major = 0;
int win_version_minor = 0;
} // namespace Global

string current_action           = "";
bool   update_check_message_box = false;
CVAR(String, dir_last, "", CVar::Flag::Save)
CVAR(Bool, update_check, true, CVar::Flag::Save)
CVAR(Bool, update_check_beta, false, CVar::Flag::Save)


// -----------------------------------------------------------------------------
//
// Classes
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// SLADELog class
//
// Extension of the wxLog class to send all wxWidgets log messages
// to the SLADE log implementation
// -----------------------------------------------------------------------------
class SLADELog : public wxLog
{
protected:
	// wx2.9.x is no longer supported.
#if (wxMAJOR_VERSION < 3)
#error This will not compile with wxWidgets older than 3.0.0 !
#endif
	void DoLogText(const wxString& msg) override
	{
		if (msg.Lower().Contains("error"))
			Log::error(msg.Right(msg.size() - 10));
		else if (msg.Lower().Contains("warning"))
			Log::warning(msg.Right(msg.size() - 10));
		else
			Log::info(msg.Right(msg.size() - 10));
	}

public:
	SLADELog()  = default;
	~SLADELog() = default;
};


// -----------------------------------------------------------------------------
// SLADEStackTrace class
//
// Extension of the wxStackWalker class that formats stack trace
// information to a multi-line string, that can be retrieved via
// getTraceString(). wxStackWalker is currently unimplemented on some
// platforms, so unfortunately it has to be disabled there
// -----------------------------------------------------------------------------
#if wxUSE_STACKWALKER
class SLADEStackTrace : public wxStackWalker
{
public:
	SLADEStackTrace() { stack_trace_ = "Stack Trace:\n"; }
	~SLADEStackTrace() = default;

	string traceString() const { return stack_trace_; }
	string topLevel() const { return top_level_; }

	void OnStackFrame(const wxStackFrame& frame) override
	{
		string location = "[unknown location] ";
		if (frame.HasSourceLocation())
			location = S_FMT("(%s:%d) ", frame.GetFileName(), frame.GetLine());

		wxUIntPtr address   = wxPtrToUInt(frame.GetAddress());
		string    func_name = frame.GetName();
		if (func_name.IsEmpty())
			func_name = S_FMT("[unknown:%d]", address);

		string line = S_FMT("%s%s", location, func_name);
		stack_trace_.Append(S_FMT("%d: %s\n", frame.GetLevel(), line));

		if (frame.GetLevel() == 0)
			top_level_ = line;
	}

private:
	string stack_trace_;
	string top_level_;
};


// -----------------------------------------------------------------------------
// SLADECrashDialog class
//
// A simple dialog that displays a crash message and a scrollable,
// multi-line textbox with a stack trace
// -----------------------------------------------------------------------------
class SLADECrashDialog : public wxDialog, public wxThreadHelper
{
public:
	SLADECrashDialog(SLADEStackTrace& st) : wxDialog(wxGetApp().GetTopWindow(), -1, "SLADE Application Crash")
	{
		top_level_ = st.topLevel();

		// Setup sizer
		wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
		SetSizer(sizer);

		wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
		sizer->Add(hbox, 0, wxEXPAND);

		// Add dead doomguy picture
		App::archiveManager()
			.programResourceArchive()
			->entryAtPath("images/STFDEAD0.png")
			->exportFile(App::path("STFDEAD0.png", App::Dir::Temp));
		wxImage img;
		img.LoadFile(App::path("STFDEAD0.png", App::Dir::Temp));
		img.Rescale(img.GetWidth(), img.GetHeight(), wxIMAGE_QUALITY_NEAREST);
		wxStaticBitmap* picture = new wxStaticBitmap(this, -1, wxBitmap(img));
		hbox->Add(picture, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxTOP | wxBOTTOM, 10);

		// Add general crash message
#ifndef NOCURL
		string message =
			"SLADE has crashed unexpectedly. To help fix the problem that caused this crash, "
			"please (optionally) enter a short description of what you were doing at the time "
			"of the crash, and click the 'Send Crash Report' button.";
#else
		string message =
			"SLADE has crashed unexpectedly. To help fix the problem that caused this crash, "
			"please email a copy of the stack trace below to sirjuddington@gmail.com, along with a "
			"description of what you were doing at the time of the crash.";
#endif
		wxStaticText* label = new wxStaticText(this, -1, message);
		hbox->Add(label, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);
		label->Wrap(480 - 20 - picture->GetSize().x);

#ifndef NOCURL
		// Add description text area
		text_description_ = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(-1, 100), wxTE_MULTILINE);
		sizer->Add(new wxStaticText(this, -1, "Description:"), 0, wxLEFT | wxRIGHT, 10);
		sizer->AddSpacer(2);
		sizer->Add(text_description_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
#endif

		// SLADE info
		if (Global::sc_rev.empty())
			trace_ = S_FMT("Version: %s\n", Global::version);
		else
			trace_ = S_FMT("Version: %s (%s)\n", Global::version, Global::sc_rev);
		if (current_action.IsEmpty())
			trace_ += "No current action\n";
		else
			trace_ += S_FMT("Current action: %s", current_action);
		trace_ += "\n";

		// System info
		OpenGL::Info gl_info = OpenGL::sysInfo();
		trace_ += "Operating System: " + wxGetOsDescription() + "\n";
		trace_ += "Graphics Vendor: " + gl_info.vendor + "\n";
		trace_ += "Graphics Hardware: " + gl_info.renderer + "\n";
		trace_ += "OpenGL Version: " + gl_info.version + "\n";

		// Stack trace
		trace_ += "\n";
		trace_ += st.traceString();

		// Last 10 log lines
		trace_ += "\nLast Log Messages:\n";
		auto& log = Log::history();
		for (auto a = log.size() - 10; a < log.size(); a++)
			trace_ += log[a].message + "\n";

		// Add stack trace text area
		text_stack_ = new wxTextCtrl(
			this, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL);
		text_stack_->SetValue(trace_);
		text_stack_->SetFont(wxFont(8, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
		sizer->Add(new wxStaticText(this, -1, "Crash Information:"), 0, wxLEFT | wxRIGHT, 10);
		sizer->AddSpacer(2);
		sizer->Add(text_stack_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

		// Dump stack trace to a file (just in case)
		wxFile file(App::path("slade3_crash.log", App::Dir::User), wxFile::write);
		file.Write(trace_);
		file.Close();

		// Also dump stack trace to console
		std::cerr << trace_;

#ifndef NOCURL
		// Add small privacy disclaimer
		string privacy =
			"Sending a crash report will only send the information displayed above, "
			"along with a copy of the logs for this session.";
		label = new wxStaticText(this, -1, privacy);
		label->Wrap(480);
		sizer->Add(label, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT | wxBOTTOM, 10);
#endif

		// Add 'Copy Stack Trace' button
		hbox = new wxBoxSizer(wxHORIZONTAL);
		sizer->Add(hbox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
		btn_copy_trace_ = new wxButton(this, -1, "Copy Stack Trace");
		hbox->AddStretchSpacer();
		hbox->Add(btn_copy_trace_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
		btn_copy_trace_->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &SLADECrashDialog::onBtnCopyTrace, this);

		// Add 'Exit SLADE' button
		btn_exit_ = new wxButton(this, -1, "Exit SLADE");
		hbox->Add(btn_exit_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
		btn_exit_->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &SLADECrashDialog::onBtnExit, this);

#ifndef NOCURL
		// Add 'Send Crash Report' button
		btn_send_ = new wxButton(this, -1, "Send Crash Report");
		hbox->Add(btn_send_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
		btn_send_->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &SLADECrashDialog::onBtnSend, this);
#endif

		Bind(wxEVT_THREAD, &SLADECrashDialog::onThreadUpdate, this);
		Bind(wxEVT_CLOSE_WINDOW, &SLADECrashDialog::onClose, this);

		// Setup layout
		wxDialog::Layout();
		SetInitialSize(wxSize(500, 600));
		CenterOnParent();
	}

	~SLADECrashDialog() = default;

	wxThread::ExitCode Entry() override
	{
		wxMailer mailer("slade.crashes@gmail.com", "qakljwqpasnmprhl", "smtp://smtp.gmail.com:587");

		// Create message
		wxEmailMessage msg;
		msg.SetFrom("SLADE");
		msg.SetTo("slade.crashes@gmail.com");
		msg.SetSubject("[" + Global::version + "] @ " + top_level_);
		msg.SetMessage(S_FMT("Description:\n%s\n\n%s", text_description_->GetValue(), trace_));
		msg.AddAttachment(App::path("slade3.log", App::Dir::User));
		msg.Finalize();

		// Send email
		bool sent = mailer.Send(msg);

		// Send event
		wxThreadEvent* evt = new wxThreadEvent();
		evt->SetInt(sent ? 1 : 0);
		wxQueueEvent(GetEventHandler(), evt);

		return (wxThread::ExitCode) nullptr;
	}

	void onBtnCopyTrace(wxCommandEvent& e)
	{
		if (wxTheClipboard->Open())
		{
			wxTheClipboard->SetData(new wxTextDataObject(trace_));
			wxTheClipboard->Flush();
			wxTheClipboard->Close();
			wxMessageBox("Stack trace successfully copied to clipboard");
		}
		else
			wxMessageBox(
				"Unable to access the system clipboard, please select+copy the text above manually",
				wxMessageBoxCaptionStr,
				wxICON_EXCLAMATION);
	}

	void onBtnSend(wxCommandEvent& e)
	{
		btn_send_->SetLabel("Sending...");
		btn_send_->Enable(false);
		btn_exit_->Enable(false);

		if (CreateThread(wxTHREAD_JOINABLE) != wxTHREAD_NO_ERROR)
			EndModal(wxID_OK);

		if (GetThread()->Run() != wxTHREAD_NO_ERROR)
			EndModal(wxID_OK);
	}

	void onBtnExit(wxCommandEvent& e) { EndModal(wxID_OK); }

	void onThreadUpdate(wxThreadEvent& e)
	{
		if (e.GetInt() == 1)
		{
			// Report sent successfully, exit
			wxMessageBox("The crash report was sent successfully, and SLADE will now close.", "Crash Report Sent");
			EndModal(wxID_OK);
		}
		else
		{
			// Sending failed
			btn_send_->SetLabel("Send Crash Report");
			btn_send_->Enable();
			btn_exit_->Enable();
			wxMessageBox(
				"The crash report failed to send. Please either try again or click 'Exit SLADE' "
				"to exit without sending.",
				"Failed to Send");
		}
	}

	void onClose(wxCloseEvent& e)
	{
		if (GetThread() && GetThread()->IsRunning())
			GetThread()->Wait();

		Destroy();
	}

private:
	wxTextCtrl* text_stack_;
	wxTextCtrl* text_description_;
	wxButton*   btn_copy_trace_;
	wxButton*   btn_exit_;
	wxButton*   btn_send_;
	string      trace_;
	string      top_level_;
};
#endif // wxUSE_STACKWALKER


// -----------------------------------------------------------------------------
// MainAppFileListener and related Classes
//
// wxWidgets IPC classes used to send filenames of archives to open
// from one SLADE instance to another in the case where a second
// instance is opened
// -----------------------------------------------------------------------------
class MainAppFLConnection : public wxConnection
{
public:
	MainAppFLConnection()  = default;
	~MainAppFLConnection() = default;

	bool OnAdvise(const wxString& topic, const wxString& item, const void* data, size_t size, wxIPCFormat format)
		override
	{
		return true;
	}

	bool OnPoke(const wxString& topic, const wxString& item, const void* data, size_t size, wxIPCFormat format) override
	{
		App::archiveManager().openArchive(item);
		return true;
	}
};

class MainAppFileListener : public wxServer
{
public:
	MainAppFileListener()  = default;
	~MainAppFileListener() = default;

	wxConnectionBase* OnAcceptConnection(const wxString& topic) override { return new MainAppFLConnection(); }
};

class MainAppFLClient : public wxClient
{
public:
	MainAppFLClient()  = default;
	~MainAppFLClient() = default;

	wxConnectionBase* OnMakeConnection() override { return new MainAppFLConnection(); }
};


// -----------------------------------------------------------------------------
//
// SLADEWxApp Class Functions
//
// -----------------------------------------------------------------------------
IMPLEMENT_APP(SLADEWxApp)


// -----------------------------------------------------------------------------
// Checks if another instance of SLADE is already running, and if so, sends the
// args to the file listener of the existing SLADE process.
// Returns false if another instance was found and the new SLADE was started
// with arguments
// -----------------------------------------------------------------------------
bool SLADEWxApp::singleInstanceCheck()
{
	single_instance_checker_ = new wxSingleInstanceChecker;

	if (argc == 1)
		return true;

	if (single_instance_checker_->IsAnotherRunning())
	{
		delete single_instance_checker_;

		// Connect to the file listener of the existing SLADE process
		auto client     = std::make_unique<MainAppFLClient>();
		auto connection = client->MakeConnection(wxGetHostName(), "SLADE_MAFL", "files");

		if (connection)
		{
			// Send args as archives to open
			for (int a = 1; a < argc; a++)
				connection->Poke(argv[a], argv[a]);

			connection->Disconnect();
		}

		return false;
	}

	return true;
}

// -----------------------------------------------------------------------------
// Application initialization, run when program is started
// -----------------------------------------------------------------------------
bool SLADEWxApp::OnInit()
{
	// Check if an instance of SLADE is already running
	if (!singleInstanceCheck())
	{
		printf("Found active instance. Quitting.\n");
		return false;
	}

	// Init global variables
	Global::error = "";

	// Init wxSocket stuff (for WebGet)
	wxSocketBase::Initialize();

	// Start up file listener
	file_listener_ = new MainAppFileListener();
	file_listener_->Create("SLADE_MAFL");

	// Setup system options
	wxSystemOptions::SetOption("mac.listctrl.always_use_generic", 1);

	// Set application name (for wx directory stuff)
#ifdef __WINDOWS__
	wxApp::SetAppName("SLADE3");
#else
    wxApp::SetAppName("slade3");
#endif

	// Handle exceptions using wxDebug stuff, but only in release mode
#ifdef NDEBUG
	wxHandleFatalExceptions(true);
#endif

	// Load image handlers
	wxInitAllImageHandlers();

#ifdef __APPLE__
	// Should be constant, wxWidgets Cocoa backend scales everything under the hood
	const double ui_scale = 1.0;
#else  // !__APPLE__
    // Calculate scaling factor (from system ppi)
    wxMemoryDC dc;
    double     ui_scale = (double)(dc.GetPPI().x) / 96.0;
    if (ui_scale < 1.)
        ui_scale = 1.;
#endif // __APPLE__

	// Get Windows version
#ifdef __WXMSW__
	wxGetOsVersion(&Global::win_version_major, &Global::win_version_minor);
	LOG_MESSAGE(1, "Windows Version: %d.%d", Global::win_version_major, Global::win_version_minor);
#endif

	// Reroute wx log messages
	wxLog::SetActiveTarget(new SLADELog());

	// Get command line arguments
	vector<string> args;
	for (int a = 1; a < argc; a++)
		args.push_back(argv[a]);

	// Init application
	if (!App::init(args, ui_scale))
		return false;

		// Check for updates
#ifdef __WXMSW__
	wxHTTP::Initialize();
	if (update_check)
		checkForUpdates(false);
#endif

	// Bind events
	Bind(wxEVT_MENU, &SLADEWxApp::onMenu, this);
	Bind(wxEVT_THREAD_WEBGET_COMPLETED, &SLADEWxApp::onVersionCheckCompleted, this);
	Bind(wxEVT_ACTIVATE_APP, &SLADEWxApp::onActivate, this);

	return true;
}

// -----------------------------------------------------------------------------
// Application shutdown, run when program is closed
// -----------------------------------------------------------------------------
int SLADEWxApp::OnExit()
{
	wxSocketBase::Shutdown();
	delete single_instance_checker_;
	delete file_listener_;

	return 0;
}

// -----------------------------------------------------------------------------
// Handler for when a fatal exception occurs - show the stack trace/crash dialog
// if it's configured to be used
// -----------------------------------------------------------------------------
void SLADEWxApp::OnFatalException()
{
#if wxUSE_STACKWALKER
#ifndef _DEBUG
	SLADEStackTrace st;
	st.WalkFromException();
	SLADECrashDialog sd(st);
	sd.ShowModal();
#endif //_DEBUG
#endif // wxUSE_STACKWALKER
}

#ifdef __APPLE__
void SLADEWxApp::MacOpenFile(const wxString& fileName)
{
	theMainWindow->archiveManagerPanel()->openFile(fileName);
}
#endif // __APPLE__

// -----------------------------------------------------------------------------
// Runs the version checker, if [message_box] is true, a message box will be
// shown if already up-to-date
// -----------------------------------------------------------------------------
void SLADEWxApp::checkForUpdates(bool message_box)
{
#ifdef __WXMSW__
	update_check_message_box = message_box;
	Log::info(1, "Checking for updates...");
	Web::getHttpAsync("slade.mancubus.net", "/version.txt", this);
#endif
}


// -----------------------------------------------------------------------------
//
// SLADEWxApp Class Events
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Called when a menu item is selected in the application
// -----------------------------------------------------------------------------
void SLADEWxApp::onMenu(wxCommandEvent& e)
{
	bool handled = false;

	// Find applicable action
	auto s_action = SAction::fromWxId(e.GetId());
	auto action   = s_action->id();

	// Handle action if valid
	if (action != "invalid")
	{
		current_action = action;
		SActionHandler::setWxIdOffset(e.GetId() - s_action->wxId());
		handled = SActionHandler::doAction(action);

		// Check if triggering object is a menu item
		if (s_action && s_action->type() == SAction::Type::Check)
		{
			if (e.GetEventObject() && e.GetEventObject()->IsKindOf(wxCLASSINFO(wxMenuItem)))
			{
				auto item = (wxMenuItem*)e.GetEventObject();
				item->Check(s_action->isChecked());
			}
		}

		current_action = "";
	}

	// If not handled, let something else handle it
	if (!handled)
		e.Skip();
}

// -----------------------------------------------------------------------------
// Called when the version check thread completes
// -----------------------------------------------------------------------------
void SLADEWxApp::onVersionCheckCompleted(wxThreadEvent& e)
{
	// Check failed
	if (e.GetString() == "connect_failed")
	{
		LOG_MESSAGE(1, "Version check failed, unable to connect");
		if (update_check_message_box)
			wxMessageBox(
				"Update check failed: unable to connect to internet. "
				"Check your connection and try again.",
				"Check for Updates");

		return;
	}

	auto info = wxSplit(e.GetString(), '\n');

	// Check for correct info
	if (info.size() != 5)
	{
		LOG_MESSAGE(1, "Version check failed, received invalid version info");
		if (update_check_message_box)
			wxMessageBox("Update check failed: received invalid version info.", "Check for Updates");
		return;
	}

	// Get version numbers
	long version_stable, version_beta, beta_num;
	info[0].ToLong(&version_stable);
	info[2].ToLong(&version_beta);
	info[3].ToLong(&beta_num);

	LOG_MESSAGE(1, "Latest stable release: v%ld \"%s\"", version_stable, info[1].Trim());
	LOG_MESSAGE(1, "Latest beta release: v%ld_b%ld \"%s\"", version_beta, beta_num, info[4].Trim());

	// Check if new stable version
	bool new_stable = false;
	if (Global::version_num < version_stable ||                          // New stable version
		(Global::version_num == version_stable && Global::beta_num > 0)) // Stable version of current beta
		new_stable = true;

	// Check if new beta version
	bool new_beta = false;
	if (version_stable < version_beta)
	{
		// Stable -> Beta
		if (Global::version_num < version_beta && Global::beta_num == 0)
			new_beta = true;

		// Beta -> Beta
		else if (
			Global::version_num < version_beta || // New version beta
			(Global::beta_num < beta_num &&       // Same version, newer beta
			 Global::version_num == version_beta && Global::beta_num > 0))
			new_beta = true;
	}

	// Set up for new beta/stable version prompt (if any)
	string message, caption, version;
	if (update_check_beta && new_beta)
	{
		// New Beta
		caption = "New Beta Version Available";
		version = info[4].Trim();
		message = S_FMT(
			"A new beta version of SLADE is available (%s), click OK to visit the SLADE homepage "
			"and download the update.",
			CHR(version));
	}
	else if (new_stable)
	{
		// New Stable
		caption = "New Version Available";
		version = info[1].Trim();
		message = S_FMT(
			"A new version of SLADE is available (%s), click OK to visit the SLADE homepage and "
			"download the update.",
			CHR(version));
	}
	else
	{
		// No update
		Log::info(1, "Already up-to-date");
		if (update_check_message_box)
			wxMessageBox("SLADE is already up to date", "Check for Updates");

		return;
	}

	// Prompt to update
	auto main_window = MainEditor::window();
	if (main_window->startPageTabOpen() && App::useWebView())
	{
		// Start Page (webview version) is open, show it there
		main_window->openStartPageTab();
		main_window->startPage()->updateAvailable(version);
	}
	else
	{
		// No start page, show a message box
		if (wxMessageBox(message, caption, wxOK | wxCANCEL) == wxOK)
			wxLaunchDefaultBrowser("http://slade.mancubus.net/index.php?page=downloads");
	}
}

// -----------------------------------------------------------------------------
// Called when the app gains focus
// -----------------------------------------------------------------------------
void SLADEWxApp::onActivate(wxActivateEvent& e)
{
	if (!e.GetActive() || App::isExiting())
	{
		e.Skip();
		return;
	}

	// Check open directory archives for changes on the file system
	if (theMainWindow && theMainWindow->archiveManagerPanel())
		theMainWindow->archiveManagerPanel()->checkDirArchives();

	e.Skip();
}


// -----------------------------------------------------------------------------
//
// Console Commands
//
// -----------------------------------------------------------------------------

CONSOLE_COMMAND(crash, 0, false)
{
	if (wxMessageBox(
			"Yes, this command does actually exist and *will* crash the program. Do you really want it to crash?",
			"...Really?",
			wxYES_NO | wxCENTRE)
		== wxYES)
	{
		uint8_t* test = nullptr;
		test[123]     = 5;
	}
}

CONSOLE_COMMAND(quit, 0, true)
{
	bool save_config = true;
	for (auto& arg : args)
	{
		if (arg.Lower() == "nosave")
			save_config = false;
	}

	App::exit(save_config);
}
