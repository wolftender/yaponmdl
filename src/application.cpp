#include <fstream>

#include <wx/aboutdlg.h>
#include <wx/dirdlg.h>
#include <wx/filename.h>

#include <stb_image/stb_image.h>

#include "application.hpp"
#include "hexview.hpp"

#include "formats/gxt.hpp"
#include "formats/gmo.hpp"
#include "formats/act.hpp"
#include "formats/gxx.hpp"

#include "generated/license.h"

enum MenuCommand {
    eMenuCommandFileOpenFile = 1000,
    eMenuCommandFileOpenDirectory,
    eMenuCommandFileRefreshTextures,
    eMenuCommandViewShowLogs = 2000,
    eMenuCommandViewZoomIn,
    eMenuCommandViewZoomOut,
    eMenuCommandViewResetView,
    eMenuCommandViewInterpolateGxx,
    eMenuCommandViewCamera,
    eMenuCommandViewCameraDefault,
    eMenuCommandViewCameraAzimuth,
    eMenuCommandViewCameraOrtho,
    eMenuCommandHelpLicense = 9000,
};

DirectoryViewControl::DirectoryViewControl(
    wxWindow *parent, const wxWindowID id, const wxString &root_dir, const wxString &dir, const wxPoint &pos,
    const wxSize &size, long style, const wxString &filter, int defaultFilter, const wxString &name)
    : root_{root_dir} {
    Init();
    Create(parent, id, dir, pos, size, style, filter, defaultFilter, name);
}

auto DirectoryViewControl::SetRootDirectory(const wxString &root_directory) -> void {
    root_ = root_directory;
    ReCreateTree();

    auto *tree = GetTreeCtrl();
    if (!tree) {
        return;
    }

    wxTreeItemIdValue cookie;
    const auto child = tree->GetFirstChild(GetRootId(), cookie);

    if (child.IsOk()) {
        tree->Expand(child);
    }
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

static auto MakeWindowTitle(const wxString &working_dir) -> wxString {
    return wxString{"Model browser - "} + working_dir;
}

static auto MakeLogWindow(wxWindow *parent) -> wxLogWindow * {
    auto *log_window = new wxLogWindow(parent, "Log window", false, false);
    wxLog::EnableLogging(true);
    wxLog::SetActiveTarget(log_window);
    wxLog::SetLogLevel(wxLOG_Message);
    log_window->Show(false);

    return log_window;
}

ModelBrowserFrame::ModelBrowserFrame()
    : wxFrame{nullptr,           wxID_ANY,         MakeWindowTitle(wxGetCwd()),
              wxDefaultPosition, wxSize{640, 480}, wxDEFAULT_FRAME_STYLE},
      log_window_{MakeLogWindow(this)}, working_dir_{wxGetCwd()}, texture_repository_{std::string{working_dir_}} {
    SetMinSize(wxSize{640, 480});

    // build menu
    wxMenu *menu_file = new wxMenu;
    menu_file->Append(eMenuCommandFileOpenFile, "&Open file", "Open a new file");
    menu_file->Append(eMenuCommandFileOpenDirectory, "&Open directory", "Open a new working directory");
    menu_file->AppendSeparator();
    menu_file->Append(
        eMenuCommandFileRefreshTextures, "&Refresh Texture Index", "Rescan the directory in search of textures");
    menu_file->AppendSeparator();
    menu_file->Append(wxID_EXIT);

    wxMenu *menu_view = new wxMenu;
    menu_view->Append(eMenuCommandViewShowLogs, "&Show logs", "Displays the log window");
    menu_view->AppendSeparator();
    menu_view->Append(eMenuCommandViewZoomIn, "Zoom in", "Increase zoom");
    menu_view->Append(eMenuCommandViewZoomOut, "Zoom out", "Decrease zoom");
    menu_view->Append(eMenuCommandViewResetView, "Reset view", "Restores view to the defaults");
    menu_view->AppendSeparator();

    wxMenu *camera_mode_menu = new wxMenu;
    camera_mode_menu->Append(
        eMenuCommandViewCameraDefault, "Default", "Default camera mode for given model type", wxITEM_RADIO);
    camera_mode_menu->Append(
        eMenuCommandViewCameraAzimuth, "Azimuth", "Azimuth camera with perspective projection (\"3D\" camera)",
        wxITEM_RADIO);
    camera_mode_menu->Append(
        eMenuCommandViewCameraOrtho, "Orthographic", "Orthographic projection camera (\"2D\" camera)", wxITEM_RADIO);

    menu_view->AppendSubMenu(camera_mode_menu, "Camera");
    menu_view->Append(
        eMenuCommandViewInterpolateGxx, "Interpolate GXX",
        "Enables automatic GXX interpolation (buggy for some models)", wxITEM_CHECK);

    wxMenu *menu_help = new wxMenu;
    menu_help->Append(eMenuCommandHelpLicense, "License");
    menu_help->Append(wxID_ABOUT);

    wxMenuBar *menu_bar = new wxMenuBar;
    menu_bar->Append(menu_file, "&File");
    menu_bar->Append(menu_view, "&View");
    menu_bar->Append(menu_help, "&Help");

    wxLogMessage("initialized application");

    SetMenuBar(menu_bar);
    CreateStatusBar();

    wxBoxSizer *main_sizer = new wxBoxSizer(wxHORIZONTAL);
    main_sizer->SetMinSize(900, 600);

    splitter_ =
        new ConstrainedSplitter(this, wxID_ANY, wxDefaultPosition, wxSize{500, 500}, wxSP_3D | wxSP_LIVE_UPDATE);
    splitter_->SetMinimumWindow1Size(300);
    splitter_->SetMinimumWindow2Size(600);
    splitter_->SetMinimumPaneSize(300);

    notebook_left_ = new wxNotebook(splitter_, wxID_ANY);
    notebook_right_ = new wxNotebook(splitter_, wxID_ANY);

    working_dir_ = wxGetCwd();
    dir_control_ = new DirectoryViewControl(
        notebook_left_, wxID_ANY, working_dir_, working_dir_, wxDefaultPosition, wxDefaultSize,
        wxDIRCTRL_SELECT_FIRST | wxDIRCTRL_3D_INTERNAL);
    dir_control_->SetDefaultPath(wxGetCwd());
    dir_control_->SetPath(wxGetCwd());

    notebook_left_->AddPage(dir_control_, "Directory view", true);

    main_sizer->Add(splitter_, 1, wxEXPAND, 0);
    splitter_->SplitVertically(notebook_left_, notebook_right_);

    hex_viewer_ = nullptr;

    EnableViewerOptions(false);
    SetSizerAndFit(main_sizer);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnOpenFile, this, eMenuCommandFileOpenFile);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnOpenDirectory, this, eMenuCommandFileOpenDirectory);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnRescanTextures, this, eMenuCommandFileRefreshTextures);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnShowLogWindow, this, eMenuCommandViewShowLogs);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnZoomIn, this, eMenuCommandViewZoomIn);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnZoomOut, this, eMenuCommandViewZoomOut);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnResetView, this, eMenuCommandViewResetView);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnCameraModeChanged, this, eMenuCommandViewCameraDefault);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnCameraModeChanged, this, eMenuCommandViewCameraAzimuth);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnCameraModeChanged, this, eMenuCommandViewCameraOrtho);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnEnableGxxInterpolation, this, eMenuCommandViewInterpolateGxx);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnShowLicense, this, eMenuCommandHelpLicense);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnAbout, this, wxID_ABOUT);

    dir_control_->Bind(wxEVT_DIRCTRL_FILEACTIVATED, &ModelBrowserFrame::OnFileSelected, this);
    notebook_right_->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &ModelBrowserFrame::OnPageChanged, this);

    LogTextureStats();
}

auto ModelBrowserFrame::LogTextureStats() const -> void {
    wxLogMessage(
        wxString{fmt::format(
            "texture scanning complete in {}, statistics:"
            "\n\tfound gxt files: {}"
            "\n\tfound fallback files: {}",
            std::string{working_dir_}, texture_repository_.GetNumGxtFiles(),
            texture_repository_.GetNumFallbackFiles())});
}

auto ModelBrowserFrame::RescanTextures() -> void {
    wxLogMessage("rescanning directory for new textures...");
    texture_repository_ = DirTextureRepository{std::string{working_dir_}};

    LogTextureStats();
    if (model_viewer_) {
        ReloadCurrentFile();
    }
}

auto ModelBrowserFrame::SetWorkingDirectory(const wxString &working_dir) -> void {
    working_dir_ = working_dir;
    SetTitle(MakeWindowTitle(working_dir_));
    dir_control_->SetRootDirectory(working_dir_);

    RescanTextures();
}

auto ModelBrowserFrame::CloseCurrentFile() -> void {
    EnableViewerOptions(false);

    if (hex_viewer_) {
        hex_viewer_->ClearBufferView();
        hex_viewer_->Hide();
    }

    hex_viewer_ = nullptr;
    model_viewer_ = nullptr;
    texture_viewer_ = nullptr;

    // gl context needs to be shown in order to allow for cleanup
    // this probably shold be fixed in a different way
    if (notebook_right_->GetPageCount() > 0) {
        std::vector<GLView *> gl_views;

        for (uint32_t i = 0; i < notebook_right_->GetPageCount(); ++i) {
            auto *page = notebook_right_->GetPage(i);
            auto *gl_view = dynamic_cast<GLView *>(page);

            if (gl_view) {
                gl_views.emplace_back(gl_view);
                notebook_right_->RemovePage(i);

                i = 0; // we have to restart, ids have changed
            }
        }

        for (auto *gl_view : gl_views) {
            gl_view->Reparent(this);
            gl_view->SetSize(0, 0, 1, 1);
            gl_view->Show(true);
            gl_view->Destroy();
        }
    }

    notebook_right_->DeleteAllPages();
}

auto ModelBrowserFrame::OnExit([[maybe_unused]] wxCommandEvent &event) -> void {
    Close(true);
    Destroy();
}

auto ModelBrowserFrame::OnAbout([[maybe_unused]] wxCommandEvent &event) -> void {
    wxAboutDialogInfo about_info;
    about_info.SetName("Yapon Model Viewer");
    about_info.SetVersion(wxString::Format("%s (compiled %s)", GIT_COMMIT_ID, __DATE__));
    about_info.SetDescription(
        "Patapon GXX and GMO model viewer, made by rekjn/wolftender.\nSpecial thanks to owocek.\nSpecial thanks to "
        "Patamodding Discord community.");
    about_info.SetCopyright("Copyright by rekjn (c) 2026");
    about_info.SetWebSite("https://github.com/wolftender");
    about_info.AddDeveloper("wolftender (https://github.com/wolftender)");

    wxAboutBox(about_info);
}

auto ModelBrowserFrame::OnShowLicense([[maybe_unused]] wxCommandEvent &event) -> void {
    wxDialog license_dialog{this, wxID_ANY, "License", wxDefaultPosition, wxSize{640, 700}};
    wxBoxSizer *sizer = new wxBoxSizer{wxVERTICAL};

    auto *license_text =
        new wxTextCtrl{&license_dialog,   wxID_ANY,      wxString{kLICENSE.data(), kLICENSE.size()},
                       wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP};

    sizer->Add(license_text, 1, wxEXPAND | wxALL, 10);
    sizer->Add(license_dialog.CreateButtonSizer(wxOK), 0, wxALL | wxEXPAND, 10);

    license_dialog.SetSizerAndFit(sizer);
    license_dialog.SetSize(wxSize{640, 700});
    license_dialog.ShowModal();
}

auto ModelBrowserFrame::OnOpenFile([[maybe_unused]] wxCommandEvent &event) -> void {
    wxFileDialog *file_select = new wxFileDialog(this, wxASCII_STR(wxFileSelectorPromptStr), working_dir_);
    if (file_select->ShowModal() == wxID_OK) {
        OpenNewFile(file_select->GetPath());
    }

    file_select->Destroy();
}

auto ModelBrowserFrame::OnOpenDirectory([[maybe_unused]] wxCommandEvent &event) -> void {
    wxDirDialog *dir_select = new wxDirDialog(this, wxASCII_STR(wxDirSelectorPromptStr), working_dir_);
    if (dir_select->ShowModal() == wxID_OK) {
        SetWorkingDirectory(dir_select->GetPath());
    }

    dir_select->Destroy();
}

auto ModelBrowserFrame::OnRescanTextures([[maybe_unused]] wxCommandEvent &event) -> void { RescanTextures(); }
auto ModelBrowserFrame::OnShowLogWindow([[maybe_unused]] wxCommandEvent &event) -> void { log_window_->Show(true); }

class GmoWxLogger : public gmo::GmoLogger {
public:
    auto log(std::string_view message) const -> void override {
        wxLogMessage("libgmo message: %s ", wxString{message.data(), message.size()});
    }
};

class GxxWxLogger : public gxx::GxxLogger {
public:
    auto log(std::string_view message) const -> void override {
        wxLogMessage("libgxx message: %s ", wxString{message.data(), message.size()});
    }
};

class ConvWxLogger : public conv::IConvertLogger {
public:
    auto Log(std::string_view message) const -> void override {
        wxLogMessage("libconv message: %s ", wxString{message.data(), message.size()});
    }
};

class GxtWxLogger : public gxt::GxtLogger {
public:
    auto log(std::string_view message) const -> void override {
        wxLogMessage("libgxt message: %s ", wxString{message.data(), message.size()});
    }
};

ModelBrowserFrame::DirTextureRepository::DirTextureRepository(const std::string &path) {
    if (!fs::exists(path)) {
        return;
    }

    const auto is_file = fs::is_regular_file(path);
    const auto is_dir = fs::is_directory(path);

    if (is_file) {
        const auto file_dir = fs::path{path}.parent_path();
        MapDirectory(file_dir);
    } else if (is_dir) {
        MapDirectory(fs::path{path});
    }
}

auto ModelBrowserFrame::DirTextureRepository::FetchTexture(std::string_view name) const -> std::optional<Bitmap> {
    const auto ext_position = name.find_last_of('.');
    const auto search_key = [&]() -> std::string {
        if (ext_position != std::string::npos) {
            return std::string{name.substr(0, ext_position)};
        } else {
            return std::string{name};
        }
    }();

    const auto gxt_iter = gxt_files_.find(search_key);
    if (gxt_iter != gxt_files_.end()) {
        auto gxt_res = LoadBitmapGxt(gxt_iter->second);
        if (gxt_res.has_value()) {
            return gxt_res;
        }
    }

    for (const auto &extension : kImageExtensions) {
        const auto fallback_iter = fallback_files_.find(std::string{search_key} + std::string{extension});
        if (fallback_iter == fallback_files_.end()) {
            continue;
        }

        auto res = LoadBitmapStb(fallback_iter->second);
        if (!res.has_value()) {
            continue;
        }

        return res;
    }

    return std::nullopt;
}

auto ModelBrowserFrame::DirTextureRepository::ReadWholeFileBinary(
    std::string_view name, std::vector<uint8_t> &buffer) const -> bool {
    std::fstream fs{std::string{name}, std::ios::in | std::ios::binary};

    if (!fs.good()) {
        wxLogError(wxString::Format("texture repo: failed to load file %s", name));
        return false;
    }

    fs.seekg(0, std::ios::end);
    const auto file_size = fs.tellg();
    fs.seekg(0, std::ios::beg);

    buffer.resize(file_size);
    fs.read(reinterpret_cast<char *>(buffer.data()), file_size);

    return true;
}

// fallback if gxt texture was not found
auto ModelBrowserFrame::DirTextureRepository::LoadBitmapStb(std::string_view name) const -> std::optional<Bitmap> {
    std::vector<uint8_t> buffer;
    if (!ReadWholeFileBinary(name, buffer)) {
        return std::nullopt;
    }

    int width = 0, height = 0, components = 0;
    int res = stbi_info_from_memory(buffer.data(), buffer.size(), &width, &height, &components);
    if (!res) {
        wxLogError(wxString::Format("texture repo: invalid stbi fallback image %s"));
        return std::nullopt;
    }

    auto img = stbi_load_from_memory(buffer.data(), buffer.size(), &width, &height, &components, 4);

    Bitmap bitmap;
    bitmap.width = width;
    bitmap.height = height;
    bitmap.plane.resize(width * height * 4);

    ::memcpy(bitmap.plane.data(), img, width * height * 4 * sizeof(uint8_t));
    stbi_image_free(img);

    return bitmap;
}

auto ModelBrowserFrame::DirTextureRepository::LoadBitmapGxt(std::string_view name) const -> std::optional<Bitmap> {
    std::vector<uint8_t> buffer;
    if (!ReadWholeFileBinary(name, buffer)) {
        return std::nullopt;
    }

    std::vector<gxt::GxtImageBitmap> bitmaps;
    try {
        GxtWxLogger logger;
        bitmaps = gxt::LoadBitmaps(buffer, &logger);
    } catch (const std::exception &e) {
        wxLogError(wxString::Format("texture repo: invalid gxt %s, error: %s", name, e.what()));
        return std::nullopt;
    }

    if (bitmaps.size() == 0) {
        wxLogError(wxString::Format("texture repo: gxt %s, has no bitmaps", name));
        return std::nullopt;
    }

    wxLogMessage(wxString{fmt::format("texture repo: loading texture {} from {}", name, name)});

    auto &bm = bitmaps.front();
    return Bitmap{
        .width = bm.width,
        .height = bm.height,
        .uv_scale = bm.uv_scale,
        .uv_offset = bm.uv_offset,
        .plane = std::move(bm.rgba_plane),
    };
}

auto ModelBrowserFrame::DirTextureRepository::MapDirectory(const fs::path &path) -> void {
    constexpr uint32_t kLimitFiles = 1 << 14;
    wxLogMessage(wxString::Format("texture repo: mapping directory: %s", path.c_str()));

    uint32_t num_processed = 0;
    for (const fs::directory_entry &dir_entry : fs::recursive_directory_iterator{path}) {
        if (num_processed > kLimitFiles) {
            wxLogMessage(wxString::Format("texture repo: the directory is too big to index, exceeded %d", kLimitFiles));
            return;
        }

        num_processed++;
        if (!dir_entry.is_regular_file()) {
            continue;
        }

        const auto extension = dir_entry.path().extension();
        const auto is_image_gxt = (extension == ".gxt");
        const auto is_image_any =
            std::find(kImageExtensions.begin(), kImageExtensions.end(), extension) != kImageExtensions.end();

        if (is_image_gxt) {
            const auto filename = dir_entry.path().stem().string();
            gxt_files_.emplace(std::make_pair(filename, fs::canonical(fs::absolute(dir_entry.path())).string()));
        } else if (is_image_any) {
            const auto filename = dir_entry.path().filename().string();
            fallback_files_.emplace(std::make_pair(filename, fs::canonical(fs::absolute(dir_entry.path())).string()));
        }
    }
}

class GmoLoader final : public ModelViewer::ILoader {
public:
    GmoLoader(std::span<const uint8_t> gmo_buffer, const conv::ITextureRepository &repository)
        : gmo_buffer_{gmo_buffer}, repository_{repository} {}

    virtual auto Load(render::hal::IDevice &device) const -> std::unique_ptr<ModelViewer::IModelProxy> {
        std::vector<gmo::GmoModel> gmo_model;

        try {
            GmoWxLogger logger;
            gmo_model = gmo::LoadModelFromMemory(gmo_buffer_, &logger);
        } catch (const std::exception &e) {
            wxLogError(wxString::Format("libgmo fatal error: %s", e.what()));
            return nullptr;
        }

        wxLogMessage(wxString::Format("loaded %lld models from file", gmo_model.size()));

        std::unique_ptr<render::Model> model;
        try {
            ConvWxLogger logger;
            model = conv::ConvertGMO(gmo_model.front(), &device, &repository_, &logger);
        } catch (const std::exception &e) {
            wxLogError(wxString::Format("libconv fatal error: %s", e.what()));
            return nullptr;
        }

        return std::make_unique<ModelViewer::ModelProxyModel>(std::move(model));
    }

private:
    std::span<const uint8_t> gmo_buffer_;
    const conv::ITextureRepository &repository_;
};

class ActLoader final : public ModelViewer::ILoader {
public:
    ActLoader(std::span<const uint8_t> act_buffer) : act_buffer_{act_buffer} {}

    virtual auto Load(render::hal::IDevice &device) const -> std::unique_ptr<ModelViewer::IModelProxy> {
        try {
            ConvWxLogger logger;
            const auto act_model = act::LoadFromBinary(act_buffer_);
            return std::make_unique<ModelViewer::ModelProxyModel>(conv::ConvertACT(act_model, &device, &logger));
        } catch (const std::exception &e) {
            wxLogError(wxString::Format("libact fatal error: %s", e.what()));
            return nullptr;
        }
    }

private:
    std::span<const uint8_t> act_buffer_;
};

class GxxLoader final : public ModelViewer::ILoader {
public:
    enum Flags {
        eGxxLoaderLoadAsModel = 1 << 0,
    };

    GxxLoader(std::span<const uint8_t> gxx_buffer, const conv::ITextureRepository &repository, uint32_t flags = 0)
        : gxx_buffer_{gxx_buffer}, repository_{repository}, flags_{flags} {}

    virtual auto Load([[maybe_unused]] render::hal::IDevice &device) const
        -> std::unique_ptr<ModelViewer::IModelProxy> {
        std::optional<gxx::GxxModel> gxx_model;

        try {
            GxxWxLogger logger;
            gxx_model = gxx::LoadModelFromMemory(gxx_buffer_, &logger);
        } catch (const std::exception &e) {
            wxLogError(wxString::Format("libgxx fatal error: %s", e.what()));
            return nullptr;
        }

        wxLogMessage("finished loading gxx model from file");

        const auto load_as_model = (flags_ & eGxxLoaderLoadAsModel) == eGxxLoaderLoadAsModel;
        if (load_as_model) {
            wxLogMessage("gxx load as model enabled!");

            std::unique_ptr<render::Model> model;
            try {
                ConvWxLogger logger;
                model = conv::ConvertGXX(gxx_model.value(), &device, &repository_, &logger);
            } catch (const std::exception &e) {
                wxLogError(wxString::Format("libconv fatal error: %s", e.what()));
                return nullptr;
            }

            return std::make_unique<ModelViewer::ModelProxyModel>(std::move(model));
        }

        std::unique_ptr<render::Drawlist> drawlist;
        try {
            ConvWxLogger logger;
            drawlist = conv::ConvertGXXDrawlist(gxx_model.value(), &device, &repository_, &logger);
        } catch (const std::exception &e) {
            wxLogError(wxString::Format("libconv fatal error: %s", e.what()));
            return nullptr;
        }

        return std::make_unique<ModelViewer::ModelProxyDrawlist>(std::move(drawlist));
    }

private:
    std::span<const uint8_t> gxx_buffer_;
    const conv::ITextureRepository &repository_;
    uint32_t flags_ = 0;
};

auto ModelBrowserFrame::OnFileSelected([[maybe_unused]] wxCommandEvent &event) -> void {
    const auto full_path = dir_control_->GetFilePath();
    OpenNewFile(full_path);
}

auto ModelBrowserFrame::OpenNewFile(const wxString &full_path) -> void {
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

    HandleNewFile();
}

auto ModelBrowserFrame::ReloadCurrentFile() -> void {
    CloseCurrentFile();
    HandleNewFile();
}

auto ModelBrowserFrame::HandleNewFile() -> void {
    hex_viewer_ = new HexViewer(notebook_right_, wxID_ANY, wxDefaultPosition, wxDefaultSize, current_file_);
    hex_viewer_->SetBufferView(current_file_);
    hex_viewer_->Show();

    if (gxt::CheckHeader(current_file_)) {
        texture_viewer_ = new TextureViewer(notebook_right_, GLView::CreateAttributes(), current_file_);
        notebook_right_->AddPage(texture_viewer_, "Texture view", true);
    } else if (gmo::CheckHeader(current_file_)) {
        default_camera_view_ = CameraMode::eOrthographic;
        model_viewer_ = new ModelDisplay(
            std::make_unique<GmoLoader>(current_file_, texture_repository_), CreateCameraController(), notebook_right_);
        notebook_right_->AddPage(model_viewer_, "GMO Model view", true);
    } else if (act::CheckHeader(current_file_)) {
        default_camera_view_ = CameraMode::eAzimuth;
        model_viewer_ =
            new ModelDisplay(std::make_unique<ActLoader>(current_file_), CreateCameraController(), notebook_right_);
        notebook_right_->AddPage(model_viewer_, "ACT Model view", true);
    } else if (gxx::CheckHeader(current_file_)) {
        default_camera_view_ = CameraMode::eOrthographic;
        uint32_t gxx_load_flags = 0;
        if (preferences_.enable_gxx_interpolation) {
            gxx_load_flags |= GxxLoader::eGxxLoaderLoadAsModel;
        }

        model_viewer_ = new ModelDisplay(
            std::make_unique<GxxLoader>(current_file_, texture_repository_, gxx_load_flags), CreateCameraController(),
            notebook_right_);
        notebook_right_->AddPage(model_viewer_, "GXX Model view", true);
    }

    notebook_right_->AddPage(hex_viewer_, "Hex view", false);
}

auto ModelBrowserFrame::OnPageChanged([[maybe_unused]] wxBookCtrlEvent &event) -> void {
    auto *widget = notebook_right_->GetCurrentPage();

    if (widget == model_viewer_ && model_viewer_ != nullptr) {
        EnableViewerOptions(true);
    } else if (widget == texture_viewer_ && texture_viewer_ != nullptr) {
        EnableViewerOptions(true);
    } else {
        EnableViewerOptions(false);
    }
}

auto ModelBrowserFrame::OnZoomOut([[maybe_unused]] wxCommandEvent &event) -> void {
    if (model_viewer_) {
        model_viewer_->GetViewer()->ZoomOut();
    } else if (texture_viewer_) {
        texture_viewer_->ZoomOut();
    }
}

auto ModelBrowserFrame::OnZoomIn([[maybe_unused]] wxCommandEvent &event) -> void {
    if (model_viewer_) {
        model_viewer_->GetViewer()->ZoomIn();
    } else if (texture_viewer_) {
        texture_viewer_->ZoomIn();
    }
}

auto ModelBrowserFrame::OnResetView([[maybe_unused]] wxCommandEvent &event) -> void {
    if (model_viewer_) {
        model_viewer_->GetViewer()->ResetView();
    } else if (texture_viewer_) {
        texture_viewer_->ResetView();
    }
}

auto ModelBrowserFrame::OnCameraModeChanged(wxCommandEvent &event) -> void {
    const auto is_enable = event.IsChecked();
    const auto mode_idx = event.GetId();

    if (!is_enable) {
        return;
    }

    switch (mode_idx) {
    case eMenuCommandViewCameraDefault:
        UpdateCameraMode(CameraMode::eDefault);
        break;
    case eMenuCommandViewCameraAzimuth:
        UpdateCameraMode(CameraMode::eAzimuth);
        break;
    case eMenuCommandViewCameraOrtho:
        UpdateCameraMode(CameraMode::eOrthographic);
        break;
    default:
        break;
    }
}

auto ModelBrowserFrame::OnEnableGxxInterpolation(wxCommandEvent &event) -> void {
    preferences_.enable_gxx_interpolation = event.IsChecked();

    if (model_viewer_) {
        ReloadCurrentFile();
    }
}

auto ModelBrowserFrame::UpdateCameraMode(CameraMode mode) -> void {
    if (preferences_.camera_mode == mode) {
        return;
    }

    preferences_.camera_mode = mode;

    if (model_viewer_) {
        model_viewer_->GetViewer()->SetCameraController(CreateCameraController());
    }
}

auto ModelBrowserFrame::CreateCameraController() const -> std::unique_ptr<ModelViewer::ICameraController> {
    const auto type =
        preferences_.camera_mode == CameraMode::eDefault ? default_camera_view_ : preferences_.camera_mode;

    switch (type) {
    case CameraMode::eOrthographic:
        return ModelViewer::MakeOrthoCamera();
    default:
        return ModelViewer::MakeAzimuthCamera();
    }
}

auto ModelBrowserFrame::EnableViewerOptions(bool enable) -> void {
    GetMenuBar()->Enable(eMenuCommandViewZoomIn, enable);
    GetMenuBar()->Enable(eMenuCommandViewZoomOut, enable);
    GetMenuBar()->Enable(eMenuCommandViewResetView, enable);
}

wxIMPLEMENT_APP(ModelBrowserApplication);
