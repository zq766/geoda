/**
 * GeoDa TM, Copyright (C) 2011-2015 by Luc Anselin - all rights reserved
 *
 * This file is part of GeoDa.
 * 
 * GeoDa is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GeoDa is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <set>
#include <boost/foreach.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <wx/filename.h>
#include <wx/filedlg.h>
#include <wx/textdlg.h>
#include <wx/settings.h>
#include <wx/valnum.h>
#include <wx/valtext.h>
#include <wx/xrc/xmlres.h>
#include "../GdaConst.h"
#include "../GeneralWxUtils.h"
#include "../Project.h"
#include "../SaveButtonManager.h"
#include "../DataViewer/TableInterface.h"
#include "../Explore/ConnectivityHistView.h"
#include "../Explore/ConnectivityMapView.h"
#include "../HighlightState.h"
#include "../ShapeOperations/GeodaWeight.h"
#include "../ShapeOperations/GwtWeight.h"
#include "../ShapeOperations/GalWeight.h"
#include "../ShapeOperations/WeightsManState.h"
#include "../ShapeOperations/WeightUtils.h"
#include "../ShapeOperations/WeightsManager.h"
#include "../logger.h"
#include "../GeoDa.h"
#include "../io/arcgis_swm.h"
#include "../io/matlab_mat.h"
#include "../io/weights_interface.h"
#include "WeightsManDlg.h"

BEGIN_EVENT_TABLE(WeightsManFrame, TemplateFrame)
	EVT_ACTIVATE(WeightsManFrame::OnActivate)
END_EVENT_TABLE()

WeightsManFrame::WeightsManFrame(wxFrame *parent, Project* project,
								 const wxString& title, const wxPoint& pos,
								 const wxSize& size, const long style)
: TemplateFrame(parent, project, title, pos, size, style),
conn_hist_canvas(0),
conn_map_canvas(0),
project_p(project),
w_man_int(project->GetWManInt()), w_man_state(project->GetWManState()),
table_int(project->GetTableInt()), suspend_w_man_state_updates(false),
create_btn(0), load_btn(0), remove_btn(0), w_list(0)
{
	wxLogMessage("Entering WeightsManFrame::WeightsManFrame");
	
	panel = new wxPanel(this);
	
    if (!wxSystemSettings::GetAppearance().IsDark()) {
        panel->SetBackgroundColour(*wxWHITE);
        SetBackgroundColour(*wxWHITE);
    }
	
    // next 2 lines for time editor - incorrect number of views open #1754
    bool is_any_time_variant = false;
    SetDependsOnNonSimpleGroups(is_any_time_variant);
    
	create_btn = new wxButton(panel, XRCID("ID_CREATE_BTN"), _("Create"),
                            wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	load_btn = new wxButton(panel, XRCID("ID_LOAD_BTN"), _("Load"),
                            wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	remove_btn = new wxButton(panel, XRCID("ID_REMOVE_BTN"), _("Remove"),
                            wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    histogram_btn = new wxButton(panel, XRCID("ID_HISTOGRAM_BTN"),
                            _("Histogram"), wxDefaultPosition,
                            wxDefaultSize, wxBU_EXACTFIT);
    connectivity_map_btn = new wxButton(panel, XRCID("ID_CONNECT_MAP_BTN"),
                            _("Connectivity Map"), wxDefaultPosition,
                            wxDefaultSize, wxBU_EXACTFIT);
    connectivity_graph_btn = new wxButton(panel, XRCID("ID_CONNECT_GRAPH_BTN"),
                            _("Connectivity Graph"), wxDefaultPosition,
                            wxDefaultSize, wxBU_EXACTFIT);
    intersection_btn = new wxButton(panel, XRCID("ID_INTERSECTION_BTN"),
                            _("Intersection"), wxDefaultPosition,
                            wxDefaultSize, wxBU_EXACTFIT);
    union_btn = new wxButton(panel, XRCID("ID_UNION_BTN"),
                            _("Union"), wxDefaultPosition,
                            wxDefaultSize, wxBU_EXACTFIT);
    symmetric_btn = new wxButton(panel, XRCID("ID_SYMMETRIC_BTN"),
                             _("Make Symmetric"), wxDefaultPosition,
                             wxDefaultSize, wxBU_EXACTFIT);
    mutual_chk = new wxCheckBox(panel, XRCID("ID_MUTUAL_CHK"),
                                _("mutual"));
	Connect(XRCID("ID_CREATE_BTN"), wxEVT_BUTTON,
            wxCommandEventHandler(WeightsManFrame::OnCreateBtn));
	Connect(XRCID("ID_LOAD_BTN"), wxEVT_BUTTON,
            wxCommandEventHandler(WeightsManFrame::OnLoadBtn));
	Connect(XRCID("ID_REMOVE_BTN"), wxEVT_BUTTON,
            wxCommandEventHandler(WeightsManFrame::OnRemoveBtn));
    Connect(XRCID("ID_HISTOGRAM_BTN"), wxEVT_BUTTON,
            wxCommandEventHandler(WeightsManFrame::OnHistogramBtn));
    Connect(XRCID("ID_CONNECT_MAP_BTN"), wxEVT_BUTTON,
            wxCommandEventHandler(WeightsManFrame::OnConnectMapBtn));
    Connect(XRCID("ID_CONNECT_GRAPH_BTN"), wxEVT_BUTTON,
            wxCommandEventHandler(WeightsManFrame::OnConnectGraphBtn));
    Connect(XRCID("ID_INTERSECTION_BTN"), wxEVT_BUTTON,
            wxCommandEventHandler(WeightsManFrame::OnIntersectionBtn));
    Connect(XRCID("ID_UNION_BTN"), wxEVT_BUTTON,
            wxCommandEventHandler(WeightsManFrame::OnUnionBtn));
    Connect(XRCID("ID_SYMMETRIC_BTN"), wxEVT_BUTTON,
            wxCommandEventHandler(WeightsManFrame::OnSymmetricBtn));
	w_list = new wxListCtrl(panel, XRCID("ID_W_LIST"), wxDefaultPosition,
            wxSize(-1, 100), wxLC_REPORT);
	// Note: search for "ungrouped_list" for examples of wxListCtrl usage.
	w_list->AppendColumn(_("Weights Name"));
	w_list->SetColumnWidth(TITLE_COL, 300);
	InitWeightsList();
	
	Connect(XRCID("ID_W_LIST"), wxEVT_LIST_ITEM_SELECTED,
            wxListEventHandler(WeightsManFrame::OnWListItemSelect));
	Connect(XRCID("ID_W_LIST"), wxEVT_LIST_ITEM_DESELECTED,
            wxListEventHandler(WeightsManFrame::OnWListItemDeselect));
	
	details_win = wxWebView::New(panel, wxID_ANY, wxWebViewDefaultURLStr,
            wxDefaultPosition, wxSize(-1, 200));

	// Arrange above widgets in panel using sizers.
	// Top level panel sizer will be panel_h_szr
	// Below that will be panel_v_szr
	// panel_v_szr will directly receive widgets
	
	wxBoxSizer* btns_row1_h_szr = new wxBoxSizer(wxHORIZONTAL);
	btns_row1_h_szr->Add(create_btn, 0, wxALIGN_CENTER_VERTICAL);
	btns_row1_h_szr->AddSpacer(5);
	btns_row1_h_szr->Add(load_btn, 0, wxALIGN_CENTER_VERTICAL);
	btns_row1_h_szr->AddSpacer(5);
	btns_row1_h_szr->Add(remove_btn, 0, wxALIGN_CENTER_VERTICAL);
	
    wxBoxSizer* btns_row2_h_szr = new wxBoxSizer(wxHORIZONTAL);
    btns_row2_h_szr->Add(histogram_btn, 0, wxALIGN_CENTER_VERTICAL);
    btns_row2_h_szr->AddSpacer(5);
    btns_row2_h_szr->Add(connectivity_map_btn, 0, wxALIGN_CENTER_VERTICAL);
    btns_row2_h_szr->AddSpacer(5);
    btns_row2_h_szr->Add(connectivity_graph_btn, 0, wxALIGN_CENTER_VERTICAL);
    btns_row2_h_szr->AddSpacer(5);

    wxBoxSizer* btns_row3_h_szr = new wxBoxSizer(wxHORIZONTAL);
    btns_row3_h_szr->Add(intersection_btn, 0, wxALIGN_CENTER_VERTICAL);
    btns_row3_h_szr->AddSpacer(5);
    btns_row3_h_szr->Add(union_btn, 0, wxALIGN_CENTER_VERTICAL);
    btns_row3_h_szr->AddSpacer(5);
    btns_row3_h_szr->Add(symmetric_btn, 0, wxALIGN_CENTER_VERTICAL);
    btns_row3_h_szr->Add(mutual_chk, 0, wxALIGN_CENTER_VERTICAL);
    btns_row3_h_szr->AddSpacer(5);


	wxBoxSizer* wghts_list_h_szr = new wxBoxSizer(wxHORIZONTAL);
	wghts_list_h_szr->Add(w_list);
	
	wxBoxSizer* panel_v_szr = new wxBoxSizer(wxVERTICAL);
	panel_v_szr->Add(btns_row1_h_szr, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
    panel_v_szr->AddSpacer(15);
	panel_v_szr->Add(wghts_list_h_szr, 0, wxALIGN_CENTER_HORIZONTAL);
    panel_v_szr->Add(btns_row3_h_szr, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
	panel_v_szr->Add(details_win, 1, wxEXPAND);
	panel_v_szr->Add(btns_row2_h_szr, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

	wxBoxSizer* panel_h_szr = new wxBoxSizer(wxHORIZONTAL);
	panel_h_szr->Add(panel_v_szr, 1, wxEXPAND);
	
	panel->SetSizer(panel_h_szr);
	
	boost::uuids::uuid default_id = w_man_int->GetDefault();
	SelectId(default_id);
	UpdateButtons();
	
	// Top Sizer for Frame
	wxBoxSizer* top_h_sizer = new wxBoxSizer(wxHORIZONTAL);
	top_h_sizer->Add(panel, 1, wxEXPAND|wxALL, 8);
	//top_h_sizer->Add(right_v_szr, 1, wxEXPAND);
	
	wxColour panel_color = panel->GetBackgroundColour();
	SetBackgroundColour(panel_color);
	//hist_canvas->SetCanvasBackgroundColor(panel_color);
	
	SetSizerAndFit(top_h_sizer);
	DisplayStatusBar(false);

	w_man_state->registerObserver(this);
	Show(true);
	wxLogMessage("Exiting WeightsManFrame::WeightsManFrame");
}

WeightsManFrame::~WeightsManFrame()
{
	if (HasCapture()) ReleaseMouse();
	DeregisterAsActive();
	w_man_state->removeObserver(this);
}

void WeightsManFrame::OnHistogramBtn(wxCommandEvent& ev)
{
    wxLogMessage("WeightsManFrame::OnHistogramBtn()");
    boost::uuids::uuid id = GetHighlightId();
    if (id.is_nil()) return;
    // memory will be managed by wxWidgets
    ConnectivityHistFrame* f = new ConnectivityHistFrame(this, project_p, id);
}

wxString WeightsManFrame::GetMapTitle(wxString title, boost::uuids::uuid w_id)
{
    wxString weights_title = w_man_int->GetTitle(w_id);
    wxString map_title = _("%s (Weights: %s)");
    map_title = wxString::Format(map_title, title, weights_title);
    return map_title;
}

void WeightsManFrame::OnConnectMapBtn(wxCommandEvent& ev)
{
    wxLogMessage("WeightsManFrame::OnConnectMapBtn()");
    boost::uuids::uuid w_id = GetHighlightId();
    if (w_id.is_nil()) return;
    
    std::vector<int> col_ids;
    std::vector<GdaVarTools::VarInfo> var_info;
    MapFrame* nf = new MapFrame(this, project_p,
                                var_info, col_ids,
                                CatClassification::no_theme,
                                MapCanvas::no_smoothing, 1,
                                w_id,
                                wxPoint(80,160),
                                GdaConst::map_default_size);
    wxString title = GetMapTitle(_("Connectivity Map"), w_id);
    nf->SetTitle(title);
    ev.SetString("Connectivity");
    nf->OnAddNeighborToSelection(ev);
}

bool WeightsManFrame::GetSelectWeights(std::vector<GeoDaWeight*>& ws)
{
    wxLogMessage("WeightsManFrame::GetSelectWeights()");
    long item = -1;
    std::set<long> items;
    std::set<long>::iterator it;
    for (long i=0; i<w_list->GetItemCount(); ++i) {
        item = w_list->GetNextItem(i-1, wxLIST_NEXT_BELOW, wxLIST_STATE_SELECTED);
        if ( item == -1 ) continue;
        items.insert(item);
    }
    std::set<wxString> id_name_set;
    for (it=items.begin(); it!=items.end(); ++it) {
        boost::uuids::uuid w_id = ids[*it];
        if (w_id.is_nil() == false) {
            GeoDaWeight* w = w_man_int->GetWeights(w_id);
            ws.push_back(w);
            id_name_set.insert(w->GetIDName());
        }
    }
    // check id: should be the same of selected weights
    return id_name_set.size() == 1;
}

void WeightsManFrame::SaveGalWeightsFile(GalWeight* new_w)
{
    wxString wildcard = _("GAL files (*.gal)|*.gal");
    wxString defaultFile(project->GetProjectTitle());
    defaultFile += ".gal";
    wxFileDialog dlg(this,
                     _("Choose an output weights file name."),
                     project->GetWorkingDir().GetPath(),
                     defaultFile,
                     wildcard,
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    wxString outputfile;
    if (dlg.ShowModal() != wxID_OK)
        return;
    outputfile = dlg.GetPath();

    int  m_num_obs = new_w->GetNumObs();
    wxString idd = new_w->GetIDName();
    wxString layer_name = project->GetProjectTitle();
    int col = table_int->FindColId(idd);
    bool flag = false;
    if (table_int->GetColType(col) == GdaConst::long64_type){
        std::vector<wxInt64> id_vec(m_num_obs);
        table_int->GetColData(col, 0, id_vec);
        flag = Gda::SaveGal(new_w->gal, layer_name, outputfile, idd, id_vec);

    } else if (table_int->GetColType(col) == GdaConst::string_type) {
        std::vector<wxString> id_vec(m_num_obs);
        table_int->GetColData(col, 0, id_vec);
        flag = Gda::SaveGal(new_w->gal, layer_name, outputfile, idd, id_vec);
    }
    if (!flag) {
        wxString msg = _("Failed to create the weights file.");
        wxMessageDialog dlg(NULL, msg, _("Error"), wxOK | wxICON_ERROR);
        dlg.ShowModal();
    } else {
        wxFileName t_ofn(outputfile);
        wxString file_name(t_ofn.GetFullName());
        wxString msg = wxString::Format(_("Weights file \"%s\" created successfully."), file_name);
        wxMessageDialog dlg(NULL, msg, _("Success"), wxOK | wxICON_INFORMATION);
        dlg.ShowModal();

        WeightUtils::LoadGalInMan(w_man_int, outputfile, table_int, idd,
                                  WeightsMetaInfo::WT_custom);
    }
}

void WeightsManFrame::OnIntersectionBtn(wxCommandEvent& ev)
{
    wxLogMessage("WeightsManFrame::OnIntersectionBtn()");
    std::vector<GeoDaWeight*> ws;
    if (WeightsManFrame::GetSelectWeights(ws)) {
        GalWeight* new_w = WeightUtils::WeightsIntersection(ws);
        SaveGalWeightsFile(new_w);
        delete new_w;
    } else {
        wxString msg = _("Selected weights are not valid for intersection, e.g. weights have different ID variable. Please select different weights.");
        wxMessageDialog dlg(NULL, msg, _("Warning"), wxOK | wxICON_INFORMATION);
        dlg.ShowModal();
    }
}

void WeightsManFrame::OnSymmetricBtn(wxCommandEvent& ev)
{
    wxLogMessage("WeightsManFrame::OnSymmetricBtn()");
    boost::uuids::uuid w_id = GetHighlightId();
    if (w_id.is_nil()) return;

    GeoDaWeight* w = w_man_int->GetWeights(w_id);

    if (w) {
        // construct new symmetric weights:  W + W' or W*W'
        std::vector<std::set<int> > nbr_dict(w->GetNumObs());
        bool is_mutual = mutual_chk->GetValue();
        for (int i=0; i<w->GetNumObs(); ++i) {
            const std::vector<long>& nbrs = w->GetNeighbors(i);
            for (int j=0; j<nbrs.size(); ++j) {
                int nbr = (int)nbrs[j];
                if (is_mutual) {
                    if (w->CheckNeighbor(nbr, i)) {
                        nbr_dict[i].insert(nbr);
                        nbr_dict[nbr].insert(i);
                    }
                } else {
                    nbr_dict[i].insert(nbr);
                    nbr_dict[nbr].insert(i);
                }
            }
        }
        // create actual GAL weights file
        std::set<int>::iterator it;
        GalElement* gal = new GalElement[w->GetNumObs()];
        for (int i=0; i<w->GetNumObs(); ++i) {
            const std::set<int>& nbrs = nbr_dict[i];
            gal[i].SetSizeNbrs(nbrs.size());
            int j=0;
            for (it = nbrs.begin(); it != nbrs.end(); ++it) {
                gal[i].SetNbr(j++, *it);
            }
        }
        GalWeight* new_w = new GalWeight();
        new_w->num_obs = w->GetNumObs();
        new_w->gal = gal;
        new_w->is_symmetric = true;
        new_w->id_field = w->GetIDName();
        SaveGalWeightsFile(new_w);
        delete new_w;
    }
}

void WeightsManFrame::OnUnionBtn(wxCommandEvent& ev)
{
    wxLogMessage("WeightsManFrame::OnUnionBtn()");
    std::vector<GeoDaWeight*> ws;
    if (WeightsManFrame::GetSelectWeights(ws)) {
        GalWeight* new_w = WeightUtils::WeightsUnion(ws);
        SaveGalWeightsFile(new_w);
        delete new_w;
    } else {
        wxString msg = _("Selected weights are not valid for union, e.g. weights have different ID variable. Please select different weights.");
        wxMessageDialog dlg(NULL, msg, _("Warning"), wxOK | wxICON_INFORMATION);
        dlg.ShowModal();
    }
}

void WeightsManFrame::OnConnectGraphBtn(wxCommandEvent& ev)
{
    wxLogMessage("WeightsManFrame::OnConnectGraphBtn()");
    boost::uuids::uuid w_id = GetHighlightId();
    if (w_id.is_nil()) return;
    
    std::vector<int> col_ids;
    std::vector<GdaVarTools::VarInfo> var_info;
    MapFrame* nf = new MapFrame(this, project_p,
                                var_info, col_ids,
                                CatClassification::no_theme,
                                MapCanvas::no_smoothing, 1,
                                w_id,
                                wxPoint(80,160),
                                GdaConst::map_default_size);
    wxString title = GetMapTitle(_("Connectivity Graph"), w_id);
    nf->SetTitle(title);
    ev.SetString("Connectivity");
    nf->OnDisplayWeightsGraph(ev);
}

void WeightsManFrame::OnActivate(wxActivateEvent& event)
{
	wxLogMessage("In WeightsManFrame::OnActivate");
	if (event.GetActive()) {
		RegisterAsActive("WeightsManFrame", GetTitle());
	}
    if ( event.GetActive() && template_canvas ) template_canvas->SetFocus();
}

void WeightsManFrame::OnWListItemSelect(wxListEvent& ev)
{
	wxLogMessage("In WeightsManFrame::OnWListItemSelect");
	long item = ev.GetIndex();
	SelectId(ids[item]);
	UpdateButtons();
	Refresh();
}

void WeightsManFrame::OnWListItemDeselect(wxListEvent& ev)
{
	//LOG_MSG("In WeightsManFrame::OnWListItemDeselect");
	//long item = ev.GetIndex();
	//LOG(item);
	//SelectId(boost::uuids::nil_uuid());
	//UpdateButtons();
	//Refresh();
}

void WeightsManFrame::OnCreateBtn(wxCommandEvent& ev)
{
	wxLogMessage("In WeightsManFrame::OnCreateBtn");
	GdaFrame::GetGdaFrame()->OnToolsWeightsCreate(ev);
}

void WeightsManFrame::OnLoadBtn(wxCommandEvent& ev)
{
	wxLogMessage("In WeightsManFrame::OnLoadBtn");
    wxFileName default_dir = project_p->GetWorkingDir();
    wxString default_path = default_dir.GetPath();
	wxFileDialog dlg( this, _("Choose Weights File"), default_path, "",
                     "Weights Files (*.gal, *.gwt, *.kwt, *.swm, *.mat)|*.gal;*.gwt;*.kwt;*.swm;*.mat");
	
    if (dlg.ShowModal() != wxID_OK) return;
	wxString path  = dlg.GetPath();
	wxString ext = GenUtils::GetFileExt(path).Lower();
	
	if (ext != "gal" && ext != "gwt" && ext != "kwt" && ext != "mat" && ext != "swm") {
		wxString msg = _("Only 'gal', 'gwt', 'kwt', 'mat' and 'swm' weights files supported.");
		wxMessageDialog dlg(this, msg, _("Error"), wxOK|wxICON_ERROR);
		dlg.ShowModal();
		return;
	}
	
	WeightsMetaInfo wmi;
    wxString id_field;
    if (ext == "mat") {
        id_field = "Unknown";
    } else if (ext == "swm") {
        id_field = ReadIdFieldFromSwm(path);
    } else {
        id_field = WeightUtils::ReadIdField(path);
    }
	wmi.SetToCustom(id_field);
	
    wmi.filename = path;
    if (path.EndsWith("kwt")) {
        wmi.weights_type = WeightsMetaInfo::WT_kernel;
    }
    
	suspend_w_man_state_updates = true;
	
	// Check if weights already loaded and simply select and set as
	// new default if already loaded.
	boost::uuids::uuid id = w_man_int->FindIdByFilename(path);
	if (id.is_nil()) {
		//id = w_man_int->FindIdByMetaInfo(wmi);
	}
	if (!id.is_nil()) {
		HighlightId(id);
		SelectId(id);
		Refresh();
		suspend_w_man_state_updates = false;
		return;
	}
	
	GalElement* tempGal = 0;
    try {
        if (ext == "gal") {
            tempGal = WeightUtils::ReadGal(path, table_int);
        } else if (ext == "swm") {
            tempGal = ReadSwmAsGal(path, table_int);
        } else if (ext == "mat") {
            tempGal = ReadMatAsGal(path, table_int);
        } else {
            tempGal = WeightUtils::ReadGwtAsGal(path, table_int);
        }
    } catch (WeightsMismatchObsException& e) {
        wxString msg = _("The number of observations specified in chosen weights file is incompatible with current Table.");
        wxMessageDialog dlg(NULL, msg, _("Error"), wxOK | wxICON_ERROR);
        dlg.ShowModal();
        tempGal = 0;
    } catch (WeightsIntegerKeyNotFoundException& e) {
        wxString msg = _("Specified key (%d) not found in currently loaded Table.");
        msg = wxString::Format(msg, e.key);
        wxMessageDialog dlg(NULL, msg, _("Error"), wxOK | wxICON_ERROR);
        dlg.ShowModal();
        tempGal = 0;
    } catch (WeightsStringKeyNotFoundException& e) {
        wxString msg = _("Specified key (%s) not found in currently loaded Table.");
        msg = wxString::Format(msg, e.key);
        wxMessageDialog dlg(NULL, msg, _("Error"), wxOK | wxICON_ERROR);
        dlg.ShowModal();
        tempGal = 0;
    } catch (WeightsIdNotFoundException& e) {
        wxString msg = _("Specified id field (%s) not found in currently loaded Table.");
        msg = wxString::Format(msg, e.id);
        wxMessageDialog dlg(NULL, msg, _("Error"), wxOK | wxICON_ERROR);
        dlg.ShowModal();
        tempGal = 0;
    } catch (WeightsNotValidException& e) {
        wxString msg = _("Weights file/format is not valid.");
        wxMessageDialog dlg(NULL, msg, _("Error"), wxOK | wxICON_ERROR);
        dlg.ShowModal();
        tempGal = 0;
    }
    
	if (tempGal == NULL) {
		// WeightsUtils read functions already reported any issues
		// to user when NULL returned.
		suspend_w_man_state_updates = false;
		return;
	}
   
    GalWeight* gw = new GalWeight();
    gw->num_obs = table_int->GetNumberRows();
    gw->wflnm = wmi.filename;
    gw->id_field = id_field;
    gw->gal = tempGal;
    
    gw->GetNbrStats();
    wmi.num_obs = gw->GetNumObs();
    wmi.SetMinNumNbrs(gw->GetMinNumNbrs());
    wmi.SetMaxNumNbrs(gw->GetMaxNumNbrs());
    wmi.SetMeanNumNbrs(gw->GetMeanNumNbrs());
    wmi.SetMedianNumNbrs(gw->GetMedianNumNbrs());
    wmi.SetSparsity(gw->GetSparsity());
    wmi.SetDensity(gw->GetDensity());
    
    id = w_man_int->RequestWeights(wmi);
    if (id.is_nil()) {
        wxString msg = _("There was a problem requesting the weights file.");
        wxMessageDialog dlg(this, msg, _("Error"), wxOK|wxICON_ERROR);
        dlg.ShowModal();
        suspend_w_man_state_updates = false;
        return;
    }
	
	if (!((WeightsNewManager*) w_man_int)->AssociateGal(id, gw)) {
		wxString msg = _("There was a problem associating the weights file.");
		wxMessageDialog dlg(this, msg, _("Error"), wxOK|wxICON_ERROR);
		dlg.ShowModal();
		delete gw;
		suspend_w_man_state_updates = false;
		return;
	}
	ids.push_back(id);
	long last = GetIdCount() - 1;
	w_list->InsertItem(last, wxEmptyString);
	w_list->SetItem(last, TITLE_COL, w_man_int->GetTitle(id));
	w_man_int->MakeDefault(id);
	HighlightId(id);
	SelectId(id);
	Refresh();
	suspend_w_man_state_updates = false;
}

int WeightsManFrame::GetIdCount()
{
    int cnt = 0;
    for (size_t i=0; i<ids.size(); ++i) {
        if (w_man_int->IsInternalUse(ids[i]) == false) {
            cnt += 1;
        }
    }
    return cnt;
}

void WeightsManFrame::OnRemoveBtn(wxCommandEvent& ev)
{
	wxLogMessage("Entering WeightsManFrame::OnRemoveBtn");
	boost::uuids::uuid id = GetHighlightId();
	if (id.is_nil()) return;
	int nb = w_man_state->NumBlockingRemoveId(id);
	if (nb > 0) {
        wxString msg = _("There is one other view open that depends on this matrix. Ok to close this view and remove?");
        if (nb > 1) {
            wxString tmp = _("There is at least one view open that depends on this matrix. Ok to close these views and remove?");
            msg = wxString::Format(tmp, nb);
        }
		wxMessageDialog dlg(this, msg, _("Notice"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
		if (dlg.ShowModal() == wxID_YES) {
			w_man_state->closeObservers(id, this);
			int nb = w_man_state->NumBlockingRemoveId(id);
			if (nb > 0) {
				// there was a problem closing some views
                wxString s = _("There is a view could not be closed. Please manually close and try again.");
                if (nb > 1) {
                    wxString tmp = _("There is at least one view could not be closed. Please manually close and try again.");
                    s = wxString::Format(tmp, nb);
                }
				wxMessageDialog dlg(this, s, _("Error"), wxICON_ERROR | wxOK);
				dlg.ShowModal();
			} else {
				w_man_int->Remove(id);
			}
		}
	} else {
		w_man_int->Remove(id);
	}
	wxLogMessage("Exiting WeightsManFrame::OnRemoveBtn");
}

/** Implementation of WeightsManStateObserver interface */
void WeightsManFrame::update(WeightsManState* o)
{
	wxLogMessage("In WeightsManFrame::update(WeightsManState* o)");
	if (suspend_w_man_state_updates) {
		return;
	}
	boost::uuids::uuid id = o->GetWeightsId();
    if (w_man_int->IsInternalUse(id)) {
        return;
    }

	if (o->GetEventType() == WeightsManState::add_evt) {
		ids.push_back(id);
		long x = w_list->InsertItem(GetIdCount(), wxEmptyString);
		if (x >= 0) {
			w_list->SetItem(x, TITLE_COL, w_man_int->GetTitle(id));
		}
		HighlightId(id);
		Refresh();

	} else if (o->GetEventType() == WeightsManState::remove_evt) {
		std::vector<boost::uuids::uuid> new_ids;
		for (size_t i=0; i<ids.size(); ++i) {
            if (w_man_int->IsInternalUse(id)) continue;
			if (ids[i] == id) {
				w_list->DeleteItem(i);
			} else {
				new_ids.push_back(ids[i]);
			}
		}
		ids = new_ids;
		if (GetIdCount() > 0) HighlightId(ids[0]);
		SelectId(GetHighlightId());

	} else if (o->GetEventType() == WeightsManState::name_change_evt) {
		for (size_t i=0; i<ids.size(); ++i) {
            if (w_man_int->IsInternalUse(id)) continue;
			if (ids[i] == id) {
				// no need to change default
				w_list->SetItem(i, TITLE_COL, w_man_int->GetTitle(ids[i]));
			}
		}
		Refresh();
	}
	UpdateButtons();
}

void WeightsManFrame::OnShowAxes(wxCommandEvent& event)
{
	wxLogMessage("In WeightsManFrame::OnShowAxes");
	if (conn_hist_canvas) {
		conn_hist_canvas->ShowAxes(!conn_hist_canvas->IsShowAxes());
		UpdateOptionMenuItems();
	}
}

void WeightsManFrame::OnDisplayStatistics(wxCommandEvent& event)
{
	wxLogMessage("In WeightsManFrame::OnDisplayStatistics");
	if (conn_hist_canvas) {
		conn_hist_canvas->DisplayStatistics(
				!conn_hist_canvas->IsDisplayStats());
		UpdateOptionMenuItems();
	}
}

void WeightsManFrame::OnHistogramIntervals(wxCommandEvent& event)
{
	wxLogMessage("In WeightsManFrame::OnDisplayStatistics");
	if (conn_hist_canvas) {
		conn_hist_canvas->HistogramIntervals();
	}
}

void WeightsManFrame::OnSaveConnectivityToTable(wxCommandEvent& event)
{
	wxLogMessage("In WeightsManFrame::OnSaveConnectivityToTable");
	if (conn_hist_canvas) {
		conn_hist_canvas->SaveConnectivityToTable();
	}
}

void WeightsManFrame::OnSelectIsolates(wxCommandEvent& event)
{
	wxLogMessage("In WeightsManFrame::OnSelectIsolates");
	if (conn_hist_canvas) {
		conn_hist_canvas->SelectIsolates();
	}
}


/** During creation of frame, load weights from weights manager.
 This should only be called once.  After initial call, list will
 be kept synchronized through WeightsStateObserver::update. */
void WeightsManFrame::InitWeightsList()
{
	w_list->DeleteAllItems();
	ids.clear();
	w_man_int->GetIds(ids);
	boost::uuids::uuid def_id = w_man_int->GetDefault();
	for (size_t i=0; i<ids.size(); ++i) {
        if (w_man_int->IsInternalUse(ids[i]) == false) {
            w_list->InsertItem(i, wxEmptyString);
            w_list->SetItem(i, TITLE_COL, w_man_int->GetTitle(ids[i]));
            if (ids[i] == def_id) {
                w_list->SetItemState(i, wxLIST_STATE_SELECTED,
                                     wxLIST_STATE_SELECTED);
            }
        }
	}
}

void WeightsManFrame::SetDetailsForId(boost::uuids::uuid id)
{
	wxLogMessage("In WeightsManFrame::SetDetailsForItem");
	if (id.is_nil() ||  w_man_int->IsInternalUse(id)) {
		SetDetailsWin(std::vector<wxString>(0), std::vector<wxString>(0));
		return;
	}
	std::vector<wxString> row_title;
	std::vector<wxString> row_content;
	
	WeightsMetaInfo wmi = w_man_int->GetMetaInfo(id);
	
	row_title.push_back(_("type"));
	row_content.push_back(wmi.TypeToStr());
    
    if (wmi.TypeToStr() == "kernel") {
        row_title.push_back(_("kernel method"));
        if (wmi.kernel.IsEmpty())
            row_content.push_back(_("unknown"));
        else
            row_content.push_back(wmi.kernel);
       
        if (wmi.bandwidth >0) {
            row_title.push_back(_("bandwidth"));
            wxString ss;
            ss << wmi.bandwidth;
            row_content.push_back(ss);
        } else  if (wmi.k > 0) {
            row_title.push_back("knn");
            wxString ss;
            ss << wmi.k;
            row_content.push_back(ss);
            if (wmi.is_adaptive_kernel) {
                row_title.push_back(_("adaptive kernel"));
                row_content.push_back( wmi.is_adaptive_kernel? _("true"):_("false"));
            }
        }
        
        if (!wmi.kernel.IsEmpty()) {
            row_title.push_back(_("kernel to diagonal"));
            row_content.push_back( wmi.use_kernel_diagnals ? _("true"):_("false"));
        }
    } else {
        if (wmi.power < 0) {
            row_title.push_back(_("inverse distance"));
            row_content.push_back(_("true"));
            row_title.push_back(_("power"));
            wxString ss;
            ss << -wmi.power;
            row_content.push_back(ss);
        }
    }
	
	row_title.push_back(_("symmetry"));
	row_content.push_back(wmi.SymToStr());
	
	row_title.push_back(_("file"));
	if (wmi.filename.IsEmpty()) {
		row_content.push_back(_("not saved"));
	} else {
        wxFileName fm(wmi.filename);
		row_content.push_back(fm.GetFullName());
	}
	
	row_title.push_back(_("id variable"));
	row_content.push_back(wmi.id_var);
	
	if (wmi.weights_type == WeightsMetaInfo::WT_rook ||
		wmi.weights_type == WeightsMetaInfo::WT_queen) {
		row_title.push_back(_("order"));
		wxString rs;
		rs << wmi.order;
		row_content.push_back(rs);
		if (wmi.order > 1) {
			row_title.push_back(_("include lower orders"));
			if (wmi.inc_lower_orders) {
				row_content.push_back(_("true"));
			} else {
				row_content.push_back(_("false"));
			}
		}
	} else if (wmi.weights_type == WeightsMetaInfo::WT_knn ||
			   wmi.weights_type == WeightsMetaInfo::WT_threshold) {
		row_title.push_back(_("distance metric"));
		row_content.push_back(wmi.DistMetricToStr());
		
		row_title.push_back(_("distance vars"));
		row_content.push_back(wmi.DistValsToStr());
		
		if (wmi.weights_type == WeightsMetaInfo::WT_threshold) {
			row_title.push_back(_("distance unit"));
			row_content.push_back(wmi.DistUnitsToStr());
		}
		
		if (wmi.weights_type == WeightsMetaInfo::WT_knn) {
			row_title.push_back(_("neighbors"));
			wxString rs;
			rs << wmi.num_neighbors;
			row_content.push_back(rs);
		} else {
			row_title.push_back(_("threshold value"));
			wxString rs;
			rs << wmi.threshold_val;
			row_content.push_back(rs);
		}
	}
    row_title.push_back(_("# observations"));
    if (wmi.num_obs >= 0)
    row_content.push_back(wxString::Format("%d", wmi.num_obs));
    else
    row_content.push_back(_("unknown"));
    
    row_title.push_back(_("min neighbors"));
    if (wmi.min_nbrs>=0)
    row_content.push_back(wxString::Format("%d", wmi.min_nbrs));
    else
    row_content.push_back(_("unknown"));
    
    row_title.push_back(_("max neighbors"));
    if (wmi.max_nbrs >= 0)
    row_content.push_back(wxString::Format("%d", wmi.max_nbrs));
    else
    row_content.push_back(_("unknown"));
    
    row_title.push_back(_("mean neighbors"));
    if (wmi.mean_nbrs>=0)
    row_content.push_back(wxString::Format("%.2f", wmi.mean_nbrs));
    else
    row_content.push_back(_("unknown"));
    
    row_title.push_back(_("median neighbors"));
    if (wmi.median_nbrs >=0)
    row_content.push_back(wxString::Format("%.2f", wmi.median_nbrs));
    else
    row_content.push_back(_("unknown"));
    
    wxString sp = _("unknown");
    if (wmi.density_val>=0)
    sp = wxString::Format("%.2f%%", wmi.density_val);
    row_title.push_back(_("% non-zero"));
    row_content.push_back(sp);
    
	SetDetailsWin(row_title, row_content);
}

void WeightsManFrame::SetDetailsWin(const std::vector<wxString>& row_title,
									const std::vector<wxString>& row_content)
{
	wxString s;
	s << "<!DOCTYPE html>";
	s << "<html>";
	s << "<head>";
	s << "  <style type=\"text/css\">";
	
	s << "  body {";
	s << "    font-family: \"Trebuchet MS\", Arial, Helvetica, sans-serif;";
	s << "    font-size: small;";
	s << "  }";
	
	s << "  h1 {";
	s << "    font-family: \"Trebuchet MS\", Arial, Helvetica, sans-serif;";
	s << "    color: blue;";
	s << "  }";
	
	s << "  #my_table {";
	s << "    font-family: \"Trebuchet MS\", Arial, Helvetica, sans-serif;";
	s << "    width: 100%;";
    s << "    border-collapse: collapse;";
	s << "  }";
	
	s << "  #my_table td, #my_table th {";
	s << "    font-size: 1em;";
	s << "    border: 1px solid #98bf21;";
	s << "    padding: 3px 7px 2px 7px;";
	s << "  }";
	
	s << "  #my_table th {";
	s << "    font-size: 1.1em;";
	s << "    text-align: left;";
	s << "    padding-top: 5px;";
	s << "    padding-bottom: 4px;";
	s << "    background-color: #A7C942;";
	s << "    color: #ffffff;";
	s << "  }";
	
	s << "  #my_table tr.alt td {";
	s << "    color: #000000;";
	s << "    background-color: #EAF2D3;";
	s << "  }";
	
	s <<   "</style>";
	s << "</head>";
	s << "<body>";
	s << "  <table id=\"my_table\">";
	s << "    <tr>";
	s << "      <th>" << _("Property") << "</th>";
	s << "      <th>" << _("Value") << "</th>";
	s << "    </tr>";
	for (size_t i=0, last=row_title.size()-1; i<last+1; ++i) {
		s << (i%2 == 0 ? "<tr>" : "<tr class=\"alt\">");
		
		s <<   "<td style=\"text-align:right; word-wrap: break-word\">";
		s <<      row_title[i] << "</td>";
		s <<   "<td>";
		if (row_title[i].CmpNoCase("file") == 0) {
			std::vector<wxString> parts;
			wxString html_formatted;
			int max_chars_per_part = 30;
			GenUtils::SplitLongPath(row_content[i], parts,
									html_formatted, max_chars_per_part);
			s << html_formatted;
		} else {
			s << row_content[i];
		}
		s <<   "</td>";
		
		s << "</tr>";
	}
	s << "  </table>";
	s << "</body>";
	s << "</html>";
	details_win->SetPage(s,"");
}

void WeightsManFrame::SelectId(boost::uuids::uuid id)
{
	w_man_int->MakeDefault(id);
	SetDetailsForId(id);
    
    if (w_man_state) {
        w_man_state->SetAddEvtTyp(id);
        w_man_state->notifyObservers(this);
    }
    
	if (conn_hist_canvas) conn_hist_canvas->ChangeWeights(id);
	if (conn_map_canvas) conn_map_canvas->ChangeWeights(id);
}

void WeightsManFrame::HighlightId(boost::uuids::uuid id)
{
	for (size_t i=0; i<ids.size(); ++i) {
        if (w_man_int->IsInternalUse(ids[i])) continue;
		if (ids[i] == id) {
			w_list->SetItemState(i, wxLIST_STATE_SELECTED,
								 wxLIST_STATE_SELECTED);
			if (w_man_int->GetDefault() != id) {
				w_man_int->MakeDefault(id);
			}
		} else {
			// unselect all other items
			w_list->SetItemState(i, 0, wxLIST_STATE_SELECTED);
		}
	}
	w_man_int->MakeDefault(id);
}

boost::uuids::uuid WeightsManFrame::GetHighlightId()
{
	if (!w_list) return boost::uuids::nil_uuid();
	for (size_t i=0, sz=w_list->GetItemCount(); i<sz; ++i) {
		if (w_list->GetItemState(i, wxLIST_STATE_SELECTED) != 0) {
			return ids[i];
		}
	}
	return boost::uuids::nil_uuid();
}

void WeightsManFrame::UpdateButtons()
{
	bool any_sel = !GetHighlightId().is_nil();
	if (remove_btn) remove_btn->Enable(any_sel);
	if (histogram_btn) histogram_btn->Enable(any_sel);
	if (connectivity_map_btn) connectivity_map_btn->Enable(any_sel);
    if (connectivity_graph_btn) connectivity_graph_btn->Enable(any_sel);
    if (project_p && project_p->isTableOnly) {
        if (connectivity_map_btn) connectivity_map_btn->Disable();
        if (connectivity_graph_btn) connectivity_graph_btn->Disable();
    }
    if (w_list) {
        int sel_w_cnt = w_list->GetSelectedItemCount();
        if (intersection_btn) intersection_btn->Enable(sel_w_cnt >= 2);
        if (union_btn) union_btn->Enable(sel_w_cnt >= 2);
        if (symmetric_btn) symmetric_btn->Enable(sel_w_cnt == 1);
        if (mutual_chk) mutual_chk->Enable(sel_w_cnt == 1);
    }
}

