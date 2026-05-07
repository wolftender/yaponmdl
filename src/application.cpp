#include <fstream>
#include <filesystem>

#include <wx/file.h>
#include <wx/filename.h>

#include "application.hpp"
#include "hexview.hpp"

#include "formats/gxt.hpp"
#include "formats/gmo.hpp"
#include "formats/act.hpp"
#include "formats/gxx.hpp"

#include "modconv/modelconvert.hpp"

namespace fs = std::filesystem;

enum MenuCommand {
    eMenuCommandFileOpenFile = 10000,
    eMenuCommandFileOpenDirectory,
    eMenuCommandViewShowLogs = 20000,
    eMenuCommandViewZoomIn,
    eMenuCommandViewZoomOut,
    eMenuCommandViewResetView,
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
    menu_view->AppendSeparator();
    menu_view->Append(eMenuCommandViewZoomIn, "Zoom in", "Increase zoom");
    menu_view->Append(eMenuCommandViewZoomOut, "Zoom out", "Decrease zoom");
    menu_view->Append(eMenuCommandViewResetView, "Reset view", "Restores view to the defaults");

    wxMenu *menu_help = new wxMenu;
    menu_help->Append(wxID_ABOUT);

    wxMenuBar *menu_bar = new wxMenuBar;
    menu_bar->Append(menu_file, "&File");
    menu_bar->Append(menu_view, "&View");
    menu_bar->Append(menu_help, "&Help");

    log_window_ = new wxLogWindow(this, "Log window", false, false);
    wxLog::EnableLogging(true);
    wxLog::SetActiveTarget(log_window_);
    wxLog::SetLogLevel(wxLOG_Message);
    log_window_->Show(false);

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
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnShowLogWindow, this, eMenuCommandViewShowLogs);
    Bind(wxEVT_MENU, &ModelBrowserFrame::OnAbout, this, wxID_ABOUT);

    dir_control_->Bind(wxEVT_DIRCTRL_FILEACTIVATED, &ModelBrowserFrame::OnFileSelected, this);
    notebook_right_->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &ModelBrowserFrame::OnPageChanged, this);
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
    wxMessageBox(
        "Yapon Model Viewer made by wolftender (https://github.com/wolftender)", "About Model viewer...",
        wxOK | wxICON_INFORMATION);
}

auto ModelBrowserFrame::OnOpenFile([[maybe_unused]] wxCommandEvent &event) -> void {}
auto ModelBrowserFrame::OnOpenDirectory([[maybe_unused]] wxCommandEvent &event) -> void {}
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

class GmoTextureRepository final : public conv::ITextureRepository {
public:
    GmoTextureRepository(const std::string &path) {
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

    auto FetchTexture(std::string_view name) const -> std::optional<Bitmap> override {
        const auto ext_position = name.find_last_of('.');
        const auto iter = [&]() {
            if (ext_position != std::string::npos) {
                return dir_tree_.find(std::string{name.substr(0, ext_position)});
            } else {
                return dir_tree_.find(std::string{name});
            }
        }();

        if (iter == dir_tree_.end()) {
            return std::nullopt;
        }

        // load bitmap
        std::fstream fs{iter->second, std::ios::in | std::ios::binary};

        if (!fs.good()) {
            wxLogError(wxString::Format("texture repo: failed to load file %s", iter->second));
            return std::nullopt;
        }

        fs.seekg(0, std::ios::end);
        const auto file_size = fs.tellg();
        fs.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer;
        buffer.resize(16);

        fs.read(reinterpret_cast<char *>(buffer.data()), 16);
        if (!gxt::CheckHeader(buffer)) {
            wxLogError(wxString::Format("texture repo: invalid texture %s", iter->second));
            return std::nullopt;
        }

        buffer.resize(file_size);
        fs.seekg(0, std::ios::beg);
        fs.read(reinterpret_cast<char *>(buffer.data()), file_size);

        std::vector<gxt::GxtImageBitmap> bitmaps;
        try {
            bitmaps = gxt::LoadBitmaps(buffer);
        } catch (const std::exception &e) {
            wxLogError(wxString::Format("texture repo: invalid gxt %s, error: %s", iter->second, e.what()));
            return std::nullopt;
        }

        if (bitmaps.size() == 0) {
            wxLogError(wxString::Format("texture repo: gxt %s, has no bitmaps", iter->second));
            return std::nullopt;
        }

        wxLogMessage(fmt::format("texture repo: loading texture {} from {}", name, iter->second));

        auto &bm = bitmaps.front();
        return Bitmap{
            .width = bm.width,
            .height = bm.height,
            .plane = std::move(bm.rgba_plane),
        };
    }

private:
    auto MapDirectory(const fs::path &path) -> void {
        constexpr uint32_t kLimitFiles = 1 << 14;
        wxLogMessage(wxString::Format("texture repo: mapping directory: %s", path.c_str()));

        uint32_t num_processed = 0;
        for (const fs::directory_entry &dir_entry : fs::recursive_directory_iterator{path}) {
            if (num_processed > kLimitFiles) {
                wxLogMessage(
                    wxString::Format("texture repo: the directory is too big to index, exceeded %d", kLimitFiles));
                return;
            }

            num_processed++;
            if (!dir_entry.is_regular_file()) {
                continue;
            }

            const auto extension = dir_entry.path().extension();
            const auto is_image =
                (extension == ".tga" || extension == ".gxt" || extension == ".png" || extension == ".jpg" ||
                 extension == ".jpeg" || extension == ".bmp");

            if (!is_image) {
                continue;
            }

            auto filename = dir_entry.path().stem().string();
            dir_tree_.emplace(std::make_pair(filename, fs::canonical(fs::absolute(dir_entry.path())).string()));
        }
    }

    std::unordered_map<std::string, std::string> dir_tree_;
};

class GmoLoader final : public ModelViewer::ILoader {
public:
    GmoLoader(std::span<const uint8_t> gmo_buffer, const std::string &directory)
        : gmo_buffer_{gmo_buffer}, directory_{directory} {}

    virtual auto Load(render::hal::IDevice &device) const -> std::unique_ptr<render::Model> {
        std::vector<gmo::GmoModel> gmo_model;
        GmoTextureRepository repository{directory_};

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
            model = conv::ConvertGMO(gmo_model.front(), &device, &repository, &logger);
        } catch (const std::exception &e) {
            wxLogError(wxString::Format("libconv fatal error: %s", e.what()));
            return nullptr;
        }

        return model;
    }

private:
    std::span<const uint8_t> gmo_buffer_;
    std::string directory_;
};

class ActLoader final : public ModelViewer::ILoader {
public:
    ActLoader(std::span<const uint8_t> act_buffer) : act_buffer_{act_buffer} {}

    virtual auto Load(render::hal::IDevice &device) const -> std::unique_ptr<render::Model> {
        try {
            ConvWxLogger logger;
            const auto act_model = act::LoadFromBinary(act_buffer_);
            return conv::ConvertACT(act_model, &device, &logger);
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
    GxxLoader(std::span<const uint8_t> gxx_buffer, const std::string &directory)
        : gxx_buffer_{gxx_buffer}, directory_{directory} {}

    virtual auto Load([[maybe_unused]] render::hal::IDevice &device) const -> std::unique_ptr<render::Model> {
        std::optional<gxx::GxxModel> gxx_model;
        GmoTextureRepository repository{directory_};

        try {
            GxxWxLogger logger;
            gxx_model = gxx::LoadModelFromMemory(gxx_buffer_, &logger);
        } catch (const std::exception &e) {
            wxLogError(wxString::Format("libgxx fatal error: %s", e.what()));
            return nullptr;
        }

        wxLogMessage("finished loading gxx model from file");

        std::unique_ptr<render::Model> model;
        try {
            ConvWxLogger logger;
            model = conv::ConvertGXX(gxx_model.value(), &device, &repository, &logger);
        } catch (const std::exception &e) {
            wxLogError(wxString::Format("libconv fatal error: %s", e.what()));
            return nullptr;
        }

        return model;
    }

private:
    std::span<const uint8_t> gxx_buffer_;
    std::string directory_;
};

auto ModelBrowserFrame::OnFileSelected([[maybe_unused]] wxCommandEvent &event) -> void {
    const auto full_path = dir_control_->GetFilePath();

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
        texture_viewer_ = new TextureViewer(notebook_right_, GLView::CreateAttributes(), current_file_);
        notebook_right_->AddPage(texture_viewer_, "Texture view", true);
    } else if (gmo::CheckHeader(current_file_)) {
        model_viewer_ = new ModelDisplay(
            std::make_unique<GmoLoader>(current_file_, std::string{working_dir_}), ModelViewer::MakeOrthoCamera(),
            notebook_right_);
        notebook_right_->AddPage(model_viewer_, "GMO Model view", true);
    } else if (act::CheckHeader(current_file_)) {
        model_viewer_ = new ModelDisplay(
            std::make_unique<ActLoader>(current_file_), ModelViewer::MakeAzimuthCamera(), notebook_right_);
        notebook_right_->AddPage(model_viewer_, "ACT Model view", true);
    } else if (gxx::CheckHeader(current_file_)) {
        model_viewer_ = new ModelDisplay(
            std::make_unique<GxxLoader>(current_file_, std::string{working_dir_}), ModelViewer::MakeOrthoCamera(),
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

auto ModelBrowserFrame::EnableViewerOptions(bool enable) -> void {
    GetMenuBar()->Enable(eMenuCommandViewZoomIn, enable);
    GetMenuBar()->Enable(eMenuCommandViewZoomOut, enable);
    GetMenuBar()->Enable(eMenuCommandViewResetView, enable);
}

wxIMPLEMENT_APP(ModelBrowserApplication);
