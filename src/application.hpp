#pragma once
#include <filesystem>

#include <wx/wx.h>
#include <wx/app.h>
#include <wx/event.h>
#include <wx/file.h>
#include <wx/dirctrl.h>
#include <wx/splitter.h>
#include <wx/notebook.h>

#include "hexview.hpp"
#include "textureview.hpp"
#include "modeldisplay.hpp"
#include "constsplitter.hpp"

#include "modconv/modelconvert.hpp"

namespace fs = std::filesystem;

class DirectoryViewControl : public wxGenericDirCtrl {
public:
    DirectoryViewControl(
        wxWindow *parent, const wxWindowID id = wxID_ANY, const wxString &root_dir = wxEmptyString,
        const wxString &dir = wxDirDialogDefaultFolderStr, const wxPoint &pos = wxDefaultPosition,
        const wxSize &size = wxDefaultSize, long style = wxDIRCTRL_3D_INTERNAL, const wxString &filter = wxEmptyString,
        int defaultFilter = 0, const wxString &name = wxTreeCtrlNameStr);

    auto SetRootDirectory(const wxString &root_directory) -> void;
    virtual auto SetupSections() -> void;

private:
    wxString root_;
};

class ModelBrowserFrame : public wxFrame {
private:
    struct Preferences {
        bool enable_gxx_interpolation = false;
    };

    class DirTextureRepository final : public conv::ITextureRepository {
    public:
        constexpr static std::array<std::string_view, 5> kImageExtensions = {".tga", ".png", ".jpg", ".jpeg", ".bmp"};

        DirTextureRepository(const std::string &path);
        auto FetchTexture(std::string_view name) const -> std::optional<Bitmap> override;

        auto GetNumGxtFiles() const -> uint32_t { return gxt_files_.size(); }
        auto GetNumFallbackFiles() const -> uint32_t { return fallback_files_.size(); }

    private:
        auto ReadWholeFileBinary(std::string_view name, std::vector<uint8_t> &buffer) const -> bool;
        auto LoadBitmapStb(std::string_view name) const -> std::optional<Bitmap>;
        auto LoadBitmapGxt(std::string_view name) const -> std::optional<Bitmap>;
        auto MapDirectory(const fs::path &path) -> void;

        std::unordered_map<std::string, std::string> gxt_files_;
        std::unordered_map<std::string, std::string> fallback_files_;
    };

public:
    ModelBrowserFrame();

private:
    auto LogTextureStats() const -> void;
    auto RescanTextures() -> void;

    auto SetWorkingDirectory(const wxString &working_dir) -> void;
    auto CloseCurrentFile() -> void;
    auto OpenNewFile(const wxString &full_path) -> void;
    auto ReloadCurrentFile() -> void;
    auto HandleNewFile() -> void;

    auto OnExit(wxCommandEvent &event) -> void;
    auto OnAbout(wxCommandEvent &event) -> void;

    auto OnOpenFile(wxCommandEvent &event) -> void;
    auto OnOpenDirectory(wxCommandEvent &event) -> void;
    auto OnRescanTextures(wxCommandEvent &event) -> void;

    auto OnShowLogWindow(wxCommandEvent &event) -> void;

    auto OnFileSelected(wxCommandEvent &event) -> void;
    auto OnPageChanged(wxBookCtrlEvent &event) -> void;

    auto OnZoomOut(wxCommandEvent &event) -> void;
    auto OnZoomIn(wxCommandEvent &event) -> void;
    auto OnResetView(wxCommandEvent &event) -> void;
    auto OnEnableGxxInterpolation(wxCommandEvent &event) -> void;

    auto EnableViewerOptions(bool enabled) -> void;

    Preferences preferences_;

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

    DirTextureRepository texture_repository_;
};

class ModelBrowserApplication : public wxApp {
public:
    ModelBrowserApplication();

    auto OnInit() -> bool override;
    auto OnExit() -> int32_t override;

private:
    ModelBrowserFrame *main_frame_ = nullptr;
};
