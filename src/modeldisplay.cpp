#include "modeldisplay.hpp"

ModelDisplay::ModelDisplay(
    std::unique_ptr<ModelViewer::ILoader> loader, wxWindow *parent, wxWindowID id, const wxPoint &position,
    const wxSize &size)
    : wxWindow{parent, id, position, size} {
    sizer_ = new wxBoxSizer(wxHORIZONTAL);

    SetMinSize(wxSize{600, 200});
    sizer_->SetMinSize(600, 200);

    viewer_ = new ModelViewer(this, GLView::CreateAttributes(), std::move(loader));

    // this callback has to be registered early because it will be called as soon as gl context is initialized
    viewer_->Bind(MODEL_VIEWER_LOADED_MODEL, &ModelDisplay::OnModelLoaded, this);
    viewer_->SetSize(1, 1);

    SetSizerAndFit(sizer_);
}

auto ModelDisplay::OnSelectAnimation(wxDataViewEvent &event) -> void {
    const auto item = event.GetItem();

    if (item.IsOk()) {
        auto *list = static_cast<wxDataViewListCtrl *>(event.GetEventObject());
        viewer_->SetAnimationIndex(list->ItemToRow(item));
    }
}

auto ModelDisplay::OnModelLoaded([[maybe_unused]] wxCommandEvent &event) -> void {
    const auto animations = viewer_->GetAnimationList();

    if (animations.size() > 0) {
        BuildAnimationLayout(animations);
    } else {
        BuildStaticLayout();
    }
}

auto ModelDisplay::BuildAnimationLayout(const std::vector<std::string> &animations) -> void {
    splitter_ = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxSize{500, 500}, wxSP_3D | wxSP_LIVE_UPDATE);
    splitter_->SetSashGravity(1.0);
    splitter_->SetSashPosition(300);
    splitter_->SetMinimumPaneSize(250);

    controls_ = new wxPanel(splitter_, wxID_ANY);
    controls_->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

    const auto window_width = GetSize().x;
    const auto right_pane_width = window_width / 4;

    sizer_->Add(splitter_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM);
    viewer_->Reparent(splitter_);
    splitter_->SplitVertically(viewer_, controls_, -right_pane_width);

    controls_->DestroyChildren();
    wxBoxSizer *controls_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBoxSizer *anim_panel = new wxStaticBoxSizer(wxVERTICAL, controls_, "Animations");
    wxDataViewListCtrl *anim_list_box = new wxDataViewListCtrl(
        controls_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES | wxDV_NO_HEADER | wxDV_SINGLE);

    anim_list_box->AppendTextColumn("Name");
    for (const auto &animation : animations) {
        anim_list_box->AppendItem({animation});
    }

    anim_list_box->SelectRow(viewer_->GetAnimationIndex());
    anim_panel->Add(anim_list_box, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM);
    controls_sizer->Add(anim_panel, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM);

    controls_->SetSizerAndFit(controls_sizer);

    anim_list_box->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &ModelDisplay::OnSelectAnimation, this);
    sizer_->Layout();
}

auto ModelDisplay::BuildStaticLayout() -> void {
    sizer_->Add(viewer_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM | wxTOP);
    sizer_->Layout();
}
