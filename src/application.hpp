#pragma once
#include <wx/wx.h>
#include <wx/app.h>
#include <wx/event.h>
#include <wx/dirctrl.h>
#include <wx/splitter.h>
#include <wx/notebook.h>

#include "hexview.hpp"
#include "textureview.hpp"
#include "modeldisplay.hpp"
#include "constsplitter.hpp"

class DirectoryViewControl : public wxGenericDirCtrl {
public:
    DirectoryViewControl(
        wxWindow *parent, const wxWindowID id = wxID_ANY, const wxString &root_dir = wxEmptyString,
        const wxString &dir = wxDirDialogDefaultFolderStr, const wxPoint &pos = wxDefaultPosition,
        const wxSize &size = wxDefaultSize, long style = wxDIRCTRL_3D_INTERNAL, const wxString &filter = wxEmptyString,
        int defaultFilter = 0, const wxString &name = wxTreeCtrlNameStr);

    virtual auto SetupSections() -> void;

private:
    wxString root_;
};

class ModelBrowserFrame : public wxFrame {
public:
    ModelBrowserFrame();

private:
    auto CloseCurrentFile() -> void;

    auto OnExit(wxCommandEvent &event) -> void;
    auto OnAbout(wxCommandEvent &event) -> void;

    auto OnOpenFile(wxCommandEvent &event) -> void;
    auto OnOpenDirectory(wxCommandEvent &event) -> void;

    auto OnShowLogWindow(wxCommandEvent &event) -> void;

    auto OnFileSelected(wxCommandEvent &event) -> void;
    auto OnPageChanged(wxBookCtrlEvent &event) -> void;

    auto EnableViewerOptions(bool enabled) -> void;

    ConstrainedSplitter *splitter_ = nullptr;
    wxNotebook *notebook_left_ = nullptr;
    wxNotebook *notebook_right_ = nullptr;

    std::vector<uint8_t> current_file_;

    ModelDisplay *model_viewer_ = nullptr;
    TextureViewer *texture_viewer_ = nullptr;

    wxLogWindow *log_window_ = nullptr;
    wxString working_dir_;
    DirectoryViewControl *dir_control_ = nullptr;
    HexViewer *hex_viewer_ = nullptr;
};

class ModelBrowserApplication : public wxApp {
public:
    ModelBrowserApplication();

    auto OnInit() -> bool override;
    auto OnExit() -> int32_t override;

private:
    ModelBrowserFrame *main_frame_ = nullptr;
};
