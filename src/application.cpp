#include <wx/file.h>

#include "application.hpp"
#include "hexview.hpp"
#include "textureview.hpp"

#include "formats/gxt.hpp"

enum MenuCommand {
    eMenuCommandFileOpenFile,
    eMenuCommandFileOpenDirectory,
    eMenuCommandViewShowLogs,
};

DirectoryViewControl::DirectoryViewControl(
    wxWindow *parent, const wxWindowID id, const wxString &root_dir, const wxString &dir, const wxPoint &pos,
    const wxSize &size, long style, const wxString &filter, int defaultFilter, const wxString &name)
    : root_{root_dir} {
    Init();
    Create(parent, id, dir, pos, size, style, filter, defaultFilter, name);
}

auto DirectoryViewControl::SetupSections() -> void {
    if (root_.IsEmpty()) {
        wxGenericDirCtrl::SetupSections();
    } else {
        AddSection(root_, wxFileNameFromPath(root_), wxFileIconsTable::folder_open);
    }
}

ModelBrowserApplication::ModelBrowserApplication() : main_frame_{nullptr} {}

auto ModelBrowserApplication::OnInit() -> bool {
#if !defined(NDEBUG) && defined(_WIN32)
    // report memory leaks on windows msvc debug
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    ModelBrowserFrame *frame = new ModelBrowserFrame();
    frame->Show(true);

    main_frame_ = frame;
    return true;
}

auto ModelBrowserApplication::OnExit() -> int32_t { return EXIT_SUCCESS; }

ModelBrowserFrame::ModelBrowserFrame()
    : wxFrame{nullptr,           wxID_ANY,         "Model browser - " + wxGetCwd(),
              wxDefaultPosition, wxSize{640, 480}, wxDEFAULT_FRAME_STYLE} {
    SetMinSize(wxSize{640, 480});

    // build menu
    wxMenu *menu_file = new wxMenu;
    menu_file->Append(eMenuCommandFileOpenFile, "&Open file", "Open a new file");
    menu_file->Append(eMenuCommandFileOpenDirectory, "&Open directory", "Open a new working directory");
    menu_file->AppendSeparator();
    menu_file->Append(wxID_EXIT);

    wxMenu *menu_view = new wxMenu;
    menu_view->Append(eMenuCommandViewShowLogs, "&Show logs", "Displays the log window");

    wxMenu *menu_help = new wxMenu;
    menu_help->Append(wxID_ABOUT);

    wxMenuBar *menu_bar = new wxMenuBar;
    menu_bar->Append(menu_file, "&File");
    menu_bar->Append(menu_view, "&View");
    menu_bar->Append(menu_help, "&Help");

    log_window_ = new wxLogWindow(nullptr, "Log window", false, false);
    wxLog::SetActiveTarget(log_window_);
    log_window_->Show(false);

    SetMenuBar(menu_bar);
    CreateStatusBar();

    wxBoxSizer *main_sizer = new wxBoxSizer(wxHORIZONTAL);
    main_sizer->SetMinSize(640, 480);

    splitter_ = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxSize{500, 500}, wxSP_3D | wxSP_LIVE_UPDATE);
    splitter_->SetMinimumPaneSize(300);

    notebook_left_ = new wxNotebook(splitter_, wxID_ANY);
    notebook_right_ = new wxNotebook(splitter_, wxID_ANY);

    dir_control_ = new DirectoryViewControl(
        notebook_left_, wxID_ANY, wxGetCwd(), wxGetCwd(), wxDefaultPosition, wxDefaultSize,
        wxDIRCTRL_SELECT_FIRST | wxDIRCTRL_3D_INTERNAL);
    dir_control_->SetDefaultPath(wxGetCwd());
    dir_control_->SetPath(wxGetCwd());

    notebook_left_->AddPage(dir_control_, "Directory view", true);

    main_sizer->Add(splitter_, 1, wxEXPAND, 0);
    splitter_->SplitVertically(notebook_left_, notebook_right_);

    hex_viewer_ = nullptr;

    SetSizerAndFit(main_sizer);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnOpenFile, this, eMenuCommandFileOpenFile);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnOpenDirectory, this, eMenuCommandFileOpenDirectory);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnShowLogWindow, this, eMenuCommandViewShowLogs);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnAbout, this, wxID_ABOUT);

    dir_control_->Bind(wxEVT_DIRCTRL_FILEACTIVATED, &ModelBrowserFrame::OnFileSelected, this);
}

auto ModelBrowserFrame::CloseCurrentFile() -> void {
    if (hex_viewer_) {
        hex_viewer_->ClearBufferView();
        hex_viewer_->Hide();
    }

    hex_viewer_ = nullptr;
    notebook_right_->DeleteAllPages();
}

auto ModelBrowserFrame::OnExit([[maybe_unused]] wxCommandEvent &event) -> void {
    Close(true);
    Destroy();
}

auto ModelBrowserFrame::OnAbout([[maybe_unused]] wxCommandEvent &event) -> void {
    wxMessageBox(
        "Yapon Model Viewer made by wolftender (https://github.com/wolftender)", "About Model viewer...",
        wxOK | wxICON_INFORMATION);
}

auto ModelBrowserFrame::OnOpenFile([[maybe_unused]] wxCommandEvent &event) -> void {}
auto ModelBrowserFrame::OnOpenDirectory([[maybe_unused]] wxCommandEvent &event) -> void {}
auto ModelBrowserFrame::OnShowLogWindow([[maybe_unused]] wxCommandEvent &event) -> void { log_window_->Show(true); }

auto ModelBrowserFrame::OnFileSelected([[maybe_unused]] wxCommandEvent &event) -> void {
    const auto full_path = dir_control_->GetFilePath();

    wxLogNull no_log;
    wxFile fs{full_path, wxFile::read};
    if (fs.Error()) {
        wxMessageBox(
            wxString::Format("Cannot open file %s (%s)", full_path, wxSysErrorMsg(fs.GetLastError())),
            "Cannot open file", wxOK | wxICON_ERROR);
        return;
    }

    // don't open files that are too big, they are most likely not psp-related anyways
    if (fs.Length() > 0x1fffffff) {
        return;
    }

    CloseCurrentFile();

    const auto file_size = fs.Length();
    current_file_.resize(file_size);

    const auto read_size = fs.Read(current_file_.data(), file_size);
    if (read_size != file_size) {
        wxMessageBox(
            wxString::Format("Cannot read file %s (%s)", full_path, wxSysErrorMsg(fs.GetLastError())),
            "Cannot open file", wxOK | wxICON_ERROR);

        current_file_.clear();
        return;
    }

    hex_viewer_ = new HexViewer(notebook_right_, wxID_ANY, wxDefaultPosition, wxDefaultSize, current_file_);
    hex_viewer_->SetBufferView(current_file_);
    hex_viewer_->Show();

    if (gxt::CheckHeader(current_file_)) {
        notebook_right_->AddPage(new TextureViewer(notebook_right_, GLView::CreateAttributes()), "Texture view", true);
    }

    notebook_right_->AddPage(hex_viewer_, "Hex view", true);
    notebook_right_->ChangeSelection(0);
}

wxIMPLEMENT_APP(ModelBrowserApplication);
