/******************************************************************************
 * $Id: latlonbounds_pi.cpp,v 1.8 2010/06/21 01:54:37 bdbcat Exp $
 *
 * Project:  OpenCPN
 * Purpose:  LatLongBounds Plugin
 * Author:   Keith Salisbury
 *
 ***************************************************************************
 *   Copyright (C) 2010 by Keith Salisbury                                 *
 *   keithsalisbury@gmail.com                                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 */


#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include <wx/aui/aui.h>

#include "latlonbounds_pi.h"


// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr)
{
    return new latlonbounds_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p)
{
    delete p;
}





//---------------------------------------------------------------------------------------------------------
//
//    LatLonBounds PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

int latlonbounds_pi::Init(void)
{
//      printf("latlonbounds_pi Init()\n");

      m_platlonbounds_window = NULL;

      // Get a pointer to the opencpn display canvas, to use as a parent for windows created
      m_parent_window = GetOCPNCanvasWindow();

      // Create the Context Menu Items

      //    In order to avoid an ASSERT on msw debug builds,
      //    we need to create a dummy menu to act as a surrogate parent of the created MenuItems
      //    The Items will be re-parented when added to the real context meenu
      wxMenu dummy_menu;

      wxMenuItem *pmi = new wxMenuItem(&dummy_menu, -1, _("Show PlugIn LatLonBoundsWindow"));
      m_show_id = AddCanvasContextMenuItem(pmi, this );
      SetCanvasContextMenuItemViz(m_show_id, true);

      wxMenuItem *pmih = new wxMenuItem(&dummy_menu, -1, _("Hide PlugIn LatLonBoundsWindow"));
      m_hide_id = AddCanvasContextMenuItem(pmih, this );
      SetCanvasContextMenuItemViz(m_hide_id, false);

        m_platlonbounds_window = new LatLonBoundsWindow(m_parent_window, wxID_ANY);

        m_AUImgr = GetFrameAuiManager();
        m_AUImgr->AddPane(m_platlonbounds_window);
        m_AUImgr->GetPane(m_platlonbounds_window).Name(_T("LatLonBounds Window Name"));

        m_AUImgr->GetPane(m_platlonbounds_window).Float();
        m_AUImgr->GetPane(m_platlonbounds_window).FloatingPosition(300,30);

        m_AUImgr->GetPane(m_platlonbounds_window).Caption(_T("AUI Managed LatLonBounds Window"));
        m_AUImgr->GetPane(m_platlonbounds_window).CaptionVisible(true);
        m_AUImgr->GetPane(m_platlonbounds_window).GripperTop(true);
        m_AUImgr->GetPane(m_platlonbounds_window).CloseButton(true);
        m_AUImgr->GetPane(m_platlonbounds_window).Show(false);
        m_AUImgr->Update();

      return (
           INSTALLS_CONTEXTMENU_ITEMS     |
           WANTS_NMEA_SENTENCES           |
           USES_AUI_MANAGER
            );
}

bool latlonbounds_pi::DeInit(void)
{
//      printf("latlonbounds_pi DeInit()\n");
      m_AUImgr->DetachPane(m_platlonbounds_window);

      if(m_platlonbounds_window)
      {
            m_platlonbounds_window->Close();
            m_platlonbounds_window->Destroy();
      }

      
      return true;
}

int latlonbounds_pi::GetAPIVersionMajor()
{
      return MY_API_VERSION_MAJOR;
}

int latlonbounds_pi::GetAPIVersionMinor()
{
      return MY_API_VERSION_MINOR;
}

int latlonbounds_pi::GetPlugInVersionMajor()
{
      return PLUGIN_VERSION_MAJOR;
}

int latlonbounds_pi::GetPlugInVersionMinor()
{
      return PLUGIN_VERSION_MINOR;
}

wxString latlonbounds_pi::GetCommonName()
{
      return _("LatLonBounds");
}

wxString latlonbounds_pi::GetShortDescription()
{
      return _("LatLonBounds PlugIn for OpenCPN");
}

wxString latlonbounds_pi::GetLongDescription()
{
      return _("LatLonBounds PlugIn for OpenCPN\n\
LatLonBoundsnstrates PlugIn processing of NMEA messages.");

}

void latlonbounds_pi::SetNMEASentence(wxString &sentence)
{
//      printf("latlonbounds_pi::SetNMEASentence\n");
      if(m_platlonbounds_window)
      {
            m_platlonbounds_window->SetSentence(sentence);
      }
}


void latlonbounds_pi::OnContextMenuItemCallback(int id)
{
      wxLogMessage(_T("latlonbounds_pi OnContextMenuCallBack()"));
     ::wxBell();

      //  Note carefully that this is a "reference to a wxAuiPaneInfo classs instance"
      //  Copy constructor (i.e. wxAuiPaneInfo pane = m_AUImgr->GetPane(m_platlonbounds_window);) will not work

      wxAuiPaneInfo &pane = m_AUImgr->GetPane(m_platlonbounds_window);
      if(!pane.IsOk())
            return;

      if(!pane.IsShown())
      {
//            printf("show\n");
            SetCanvasContextMenuItemViz(m_hide_id, true);
            SetCanvasContextMenuItemViz(m_show_id, false);

            pane.Show(true);
            m_AUImgr->Update();

      }
      else
      {
//            printf("hide\n");
            SetCanvasContextMenuItemViz(m_hide_id, false);
            SetCanvasContextMenuItemViz(m_show_id, true);

            pane.Show(false);
            m_AUImgr->Update();
      }

/*
      if(NULL == m_platlonbounds_window)
      {
            m_platlonbounds_window = new LatLonBoundsWindow(m_parent_window, wxID_ANY);

            SetCanvasContextMenuItemViz(m_hide_id, true);
            SetCanvasContextMenuItemViz(m_show_id, false);
      }
      else
      {
            m_platlonbounds_window->Close();
            m_platlonbounds_window->Destroy();
            m_platlonbounds_window = NULL;

            SetCanvasContextMenuItemViz(m_hide_id, false);
            SetCanvasContextMenuItemViz(m_show_id, true);
      }      
*/
}

void latlonbounds_pi::UpdateAuiStatus(void)
{
      //    This method is called after the PlugIn is initialized
      //    and the frame has done its initial layout, possibly from a saved wxAuiManager "Perspective"
      //    It is a chance for the PlugIn to syncronize itself internally with the state of any Panes that
      //    were added to the frame in the PlugIn ctor.

      //    We use this callback here to keep the context menu selection in sync with the window state

  
      wxAuiPaneInfo &pane = m_AUImgr->GetPane(m_platlonbounds_window);
      if(!pane.IsOk())
            return;

      printf("update %d\n",pane.IsShown());

      SetCanvasContextMenuItemViz(m_hide_id, pane.IsShown());
      SetCanvasContextMenuItemViz(m_show_id, !pane.IsShown());

}


//----------------------------------------------------------------
//
//    LatLonBounds Window Implementation
//
//----------------------------------------------------------------

BEGIN_EVENT_TABLE(LatLonBoundsWindow, wxWindow)
  EVT_PAINT ( LatLonBoundsWindow::OnPaint )
  EVT_SIZE(LatLonBoundsWindow::OnSize)


END_EVENT_TABLE()

LatLonBoundsWindow::LatLonBoundsWindow(wxWindow *pparent, wxWindowID id)
      :wxWindow(pparent, id, wxPoint(10,10), wxSize(200,200),
             wxSIMPLE_BORDER, _T("OpenCPN PlugIn"))
{
      mLat = 0.0;
      mLon = 1.0;
      mSog = 2.0;
      mCog = 3.0;
      mVar = 4.0;
}

LatLonBoundsWindow::~LatLonBoundsWindow()
{
}

void LatLonBoundsWindow::OnSize(wxSizeEvent& event)
{
      printf("LatLonBoundsWindow OnSize()\n");
}


void LatLonBoundsWindow::SetSentence(wxString &sentence)
{
      m_NMEA0183 << sentence;

      bool bGoodData = false;

      if(m_NMEA0183.PreParse())
      {
            if(m_NMEA0183.LastSentenceIDReceived == _T("RMC"))
            {
                  if(m_NMEA0183.Parse())
                  {
                              if(m_NMEA0183.Rmc.IsDataValid == NTrue)
                              {
                                    float llt = m_NMEA0183.Rmc.Position.Latitude.Latitude;
                                    int lat_deg_int = (int)(llt / 100);
                                    float lat_deg = lat_deg_int;
                                    float lat_min = llt - (lat_deg * 100);
                                    mLat = lat_deg + (lat_min/60.);
                                    if(m_NMEA0183.Rmc.Position.Latitude.Northing == South)
                                          mLat = -mLat;

                                    float lln = m_NMEA0183.Rmc.Position.Longitude.Longitude;
                                    int lon_deg_int = (int)(lln / 100);
                                    float lon_deg = lon_deg_int;
                                    float lon_min = lln - (lon_deg * 100);
                                    mLon = lon_deg + (lon_min/60.);
                                    if(m_NMEA0183.Rmc.Position.Longitude.Easting == West)
                                          mLon = -mLon;

                                    mSog = m_NMEA0183.Rmc.SpeedOverGroundKnots;
                                    mCog = m_NMEA0183.Rmc.TrackMadeGoodDegreesTrue;

                                    if(m_NMEA0183.Rmc.MagneticVariationDirection == East)
                                          mVar =  m_NMEA0183.Rmc.MagneticVariation;
                                    else if(m_NMEA0183.Rmc.MagneticVariationDirection == West)
                                          mVar = -m_NMEA0183.Rmc.MagneticVariation;

                                    bGoodData = true;

                              }
                        }
                  }
        }

      //    Got the data, now do something with it

      if(bGoodData)
      {
            Refresh(false);
      }
}

void LatLonBoundsWindow::OnPaint(wxPaintEvent& event)
{
      wxLogMessage(_T("latlonbounds_pi onpaint"));

      wxPaintDC dc ( this );

//      printf("onpaint\n");

      {
            dc.Clear();

            wxString data;
            data.Printf(_T("Lat: %g "), mLat);
            dc.DrawText(data, 10, 10);

            data.Printf(_T("Lon: %g"), mLon);
            dc.DrawText(data, 10, 40);

            data.Printf(_T("Sog: %g"), mSog);
            dc.DrawText(data, 10, 70);

            data.Printf(_T("Cog: %g"), mCog);
            dc.DrawText(data, 10, 100);
      }
}
