/******************************************************************************
 * $Id: s57chart.cpp,v 1.14 2008/04/10 01:09:01 bdbcat Exp $
 *
 * Project:  OpenCPN
 * Purpose:  S57 Chart Object
 * Author:   David Register
 *
 ***************************************************************************
 *   Copyright (C) $YEAR$ by $AUTHOR$   *
 *   $EMAIL$   *
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
 *
<<<<<<< s57chart.cpp
 * $Log: s57chart.cpp,v $
 * Revision 1.14  2008/04/10 01:09:01  bdbcat
 * Cleanup
 *
 * Revision 1.13  2008/03/30 22:19:19  bdbcat
 * Improve messages
 *
=======
 * $Log: s57chart.cpp,v $
 * Revision 1.14  2008/04/10 01:09:01  bdbcat
 * Cleanup
 *
 * Revision 1.13  2008/03/30 22:19:19  bdbcat
 * Improve messages
 *
 * Revision 1.12  2008/01/12 06:21:18  bdbcat
 * Update for Mac OSX/Unicode
 *
>>>>>>> 1.12
 * Revision 1.11  2008/01/10 03:38:32  bdbcat
 * Update for Mac OSX
 *
 * Revision 1.9  2007/06/10 02:33:59  bdbcat
 * Color scheme support
 *
 * Revision 1.8  2007/05/03 13:23:56  dsr
 * Major refactor for 1.2.0
 *
 * Revision 1.7  2007/03/02 01:59:16  dsr
 * Convert to UTM Projection
 *
 * Revision 1.6  2007/01/19 02:18:55  dsr
 * Cleanup
 *
 * Revision 1.5  2006/10/08 00:36:44  dsr
 * no message
 *
 * Revision 1.4  2006/10/07 03:50:28  dsr
 * *** empty log message ***
 *
 * Revision 1.3  2006/10/01 03:22:59  dsr
 * no message
 *
 * Revision 1.2  2006/09/21 01:37:37  dsr
 * Major refactor/cleanup
 *
 * Revision 1.1.1.1  2006/08/21 05:52:19  dsr
 * Initial import as opencpn, GNU Automake compliant.
 *
 *
 */


// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include "wx/image.h"                           // for some reason, needed for msvc???
#include "wx/tokenzr.h"
#include <wx/textfile.h>

#include "dychart.h"

#include "s52s57.h"
#include "s52plib.h"

#include "s57chart.h"
#include "nmea.h"                               // for Pause/UnPause

#include "mygeom.h"
#include "cutil.h"
#include "georef.h"

#include "cpl_csv.h"
#include "setjmp.h"

#include "mygdal/ogr_s57.h"

CPL_CVSID("$Id: s57chart.cpp,v 1.14 2008/04/10 01:09:01 bdbcat Exp $");

extern bool GetDoubleAttr(S57Obj *obj, char *AttrName, double &val);      // found in s52cnsy


void OpenCPN_OGRErrorHandler( CPLErr eErrClass, int nError,
                              const char * pszErrorMsg );               // installed GDAL OGR library error handler

const char *MyCSVGetField( const char * pszFilename,
                         const char * pszKeyFieldName,
                         const char * pszKeyFieldValue,
                         CSVCompareCriteria eCriteria,
                         const char * pszTargetField ) ;



extern s52plib  *ps52plib;
extern S57ClassRegistrar *g_poRegistrar;
extern wxString          *g_pcsv_locn;

extern int    user_user_id;
extern int    file_user_id;


static jmp_buf env_ogrf;                    // the context saved by setjmp();

#include <wx/arrimpl.cpp>                   // Implement an array of S57 Objects
WX_DEFINE_OBJARRAY(ArrayOfS57Obj);

#include <wx/listimpl.cpp>                   // Implement a list of S57 Objects
WX_DEFINE_LIST(ListOfS57Obj);

//  This needs to be temporarily static so that S57OBJ ctor can use it to
//  convert SENC SM data to lat/lon for bounding boxes.
//  Eventually, goes into private data when chart rendering is done by
//  SM methods only.
//  At that point, S57OBJ works only in SM

static double s_ref_lat, s_ref_lon;


#define S57_THUMB_SIZE  200


//----------------------------------------------------------------------------------
//      S57Obj CTOR
//----------------------------------------------------------------------------------

S57Obj::S57Obj()
{
        attList = NULL;
        attVal = NULL;
        pPolyTessGeo = NULL;
        bCS_Added = 0;
        CSrules = NULL;
        FText = NULL;
        bFText_Added = 0;
        geoPtMulti = NULL;
        geoPtz = NULL;
        geoPt = NULL;
        bIsClone = false;
        Scamin = 10000000;                              // ten million enough?
        nRef = 0;
}

//----------------------------------------------------------------------------------
//      S57Obj DTOR
//----------------------------------------------------------------------------------

S57Obj::~S57Obj()
{
    //  Don't delete any allocated records of simple copy clones
    if(!bIsClone)
    {
        for(unsigned int iv = 0 ; iv < attVal->GetCount() ; iv++)
        {
            S57attVal *vv =  attVal->Item(iv);
            void *v2 = vv->value;
            free(v2);
            delete vv;
        }
        delete attVal;
        delete attList;


        delete pPolyTessGeo;

        if(FText)
        {
            delete FText->frmtd;    // the formatted text string
            free(FText);
        }

        if(geoPt)
            free(geoPt);
        if(geoPtz)
            free(geoPtz);
        if(geoPtMulti)
            free(geoPtMulti);
    }
}

//----------------------------------------------------------------------------------
//      S57Obj CTOR from SENC file
//----------------------------------------------------------------------------------

S57Obj::S57Obj(char *first_line, wxBufferedInputStream *pfpx)
{
    attList = NULL;
    attVal = NULL;
    pPolyTessGeo = NULL;
    bCS_Added = 0;
    CSrules = NULL;
    FText = NULL;
    bFText_Added = 0;
    bIsClone = false;

    geoPtMulti = NULL;
    geoPtz = NULL;
    geoPt = NULL;
    Scamin = 10000000;                              // ten million enough?
    nRef = 0;

    int FEIndex;

    int MAX_LINE = 499999;
    char *buf = (char *)malloc(MAX_LINE + 1);
    int llmax = 0;

    char szFeatureName[20];

    char *br;
    char szAtt[20];
    char geoMatch[20];

    bool bMulti = false;

    char *hdr_buf = (char *)malloc(1);

    if(strlen(first_line) == 0)
        return;
    strcpy(buf, first_line);

//    while(!dun)
    {

        if(!strncmp(buf, "OGRF", 4))
        {
            attList = new wxString();
            attVal =  new wxArrayOfS57attVal();

            FEIndex = atoi(buf+19);

 // Debug hooks
 //       if(!strncmp(obj->FeatureName, "DEPCNT", 6))
 //           int ffl = 4;
 //       if(FEIndex == 1226)
 //           int rrt = 5;

            strncpy(szFeatureName, buf+11, 6);
            szFeatureName[6] = 0;
            strcpy(FeatureName, szFeatureName);

    //      Build/Maintain a list of found OBJL types for later use
    //      And back-reference the appropriate list index in S57Obj for Display Filtering

            bool bNeedNew = true;
            OBJLElement *pOLE;

            for(unsigned int iPtr = 0 ; iPtr < ps52plib->pOBJLArray->GetCount() ; iPtr++)
            {
                pOLE = (OBJLElement *)(ps52plib->pOBJLArray->Item(iPtr));
                if(!strncmp(pOLE->OBJLName, szFeatureName, 6))
                {
                    iOBJL = iPtr;
                    bNeedNew = false;
                    break;
                }
            }

            if(bNeedNew)
            {
                pOLE = (OBJLElement *)malloc(sizeof(OBJLElement));
                strcpy(pOLE->OBJLName, szFeatureName);
                pOLE->nViz = 1;

                ps52plib->pOBJLArray->Add((void *)pOLE);
                iOBJL  = ps52plib->pOBJLArray->GetCount() - 1;
            }


    //      Walk thru the attributes, adding interesting ones
            int hdr_len = 0;
            char *mybuf_ptr;
            char *hdr_end;

            int prim = -1;
            int attdun = 0;

            strcpy(geoMatch, "Dummy");

            while(!attdun)
            {
                if(hdr_len)
                {
                    int nrl = my_bufgetl( mybuf_ptr, hdr_end, buf, MAX_LINE );
                    mybuf_ptr += nrl;
                    if(0 == nrl)
                    {
                        attdun = 1;
                        my_fgets(buf, MAX_LINE, *pfpx);     // this will be PolyGeo
                        break;
                    }
                }

                else
                    my_fgets(buf, MAX_LINE, *pfpx);


                if(!strncmp(buf, "HDRLEN", 6))
                {
                    hdr_len = atoi(buf+7);
                    hdr_buf = (char *)realloc(hdr_buf, hdr_len);
                    pfpx->Read(hdr_buf, hdr_len);
                    mybuf_ptr = hdr_buf;
                    hdr_end = hdr_buf + hdr_len;
                }

                else if(!strncmp(buf, geoMatch, 6))
                {
                    attdun =1;
                    break;
                }

                else if(!strncmp(buf, "  MULT", 6))         // Special multipoint
                {
                    bMulti = true;
                    attdun =1;
                    break;
                }



                else if(!strncmp(buf, "  PRIM", 6))
                {
                    prim = atoi(buf+13);
                    switch(prim)
                    {
                        case 1:
                        {
                            strcpy(geoMatch, "  POIN");
                            break;
                        }

                        case 2:                            // linestring
                        {
                            strcpy(geoMatch, "  LINE");
                            break;
                        }

                        case 3:                            // area as polygon
                        {
                            strcpy(geoMatch, "  POLY");
                            break;
                        }

                        default:                            // unrecognized
                        {
                            break;
                        }

                    }       //switch
                }               // if PRIM


                bool iua = IsUsefulAttribute(buf);

                szAtt[0] = 0;

                if(iua)
                {
                    S57attVal *pattValTmp = new S57attVal;

                    if(buf[10] == 'I')
                    {
                        br = buf+2;
                        int i=0;
                        while(*br != ' ')
                        {
                            szAtt[i++] = *br;
                            br++;
                        }

                        szAtt[i] = 0;

                        while(*br != '=')
                            br++;

                        br += 2;

                        int AValInt = atoi(br);
                        int *pAVI = (int *)malloc(sizeof(int));         //new int;
                        *pAVI = AValInt;
                        pattValTmp->valType = OGR_INT;
                        pattValTmp->value   = pAVI;

    //      Capture SCAMIN on the fly during load
                        if(!strcmp(szAtt, "SCAMIN"))
                            Scamin = AValInt;
                    }


                    else if(buf[10] == 'S')
                    {
                        strncpy(szAtt, &buf[2], 6);
                        szAtt[6] = 0;

                        br = buf + 15;

                        int nlen = strlen(br);
                        br[nlen-1] = 0;                                 // dump the NL char
                        char *pAVS = (char *)malloc(nlen + 1);          ;
                        strcpy(pAVS, br);

                        pattValTmp->valType = OGR_STR;
                        pattValTmp->value   = pAVS;
                    }


                    else if(buf[10] == 'R')
                    {
                        br = buf+2;
                        int i=0;
                        while(*br != ' ')
                        {
                            szAtt[i++] = *br;
                            br++;
                        }

                        szAtt[i] = 0;

                        while(*br != '=')
                            br++;

                        br += 2;

                        float AValfReal;
                        sscanf(br, "%f", &AValfReal);

                        double AValReal = AValfReal;        //FIXME this cast leaves trash in double

                        double *pAVR = (double *)malloc(sizeof(double));   //new double;
                        *pAVR = AValReal;

                        pattValTmp->valType = OGR_REAL;
                        pattValTmp->value   = pAVR;
                    }

                    else
                    {
                              // unknown attribute type
    //                        CPLError((CPLErr)0, 0,"Unknown Attribute Type %s", buf);
                    }


                    if(strlen(szAtt))
                    {
                        attList->Append(wxString(szAtt, wxConvUTF8));
                        attList->Append('\037');

                        attVal->Add(pattValTmp);
                    }
                    else
                        delete pattValTmp;

                }        //useful
            }               // while attdun



    //              Develop Geometry

            switch(prim)
            {
                case 1:
                {
                    if(!bMulti)
                    {
                        Primitive_type = GEO_POINT;

                        my_fgets(buf, MAX_LINE, *pfpx);
                        int wkb_len = atoi(buf+2);
                        pfpx->Read(buf,  wkb_len);

                        npt  = 1;
                        float *pfs = (float *)(buf + 5);                // point to the point

                        float easting, northing;
                        easting = *pfs++;
                        northing = *pfs;

                        x = easting;                                    // and save as SM
                        y = northing;

                        //  Convert from SM to lat/lon for bbox
                        double xll, yll;
                        fromSM(easting, northing, s_ref_lat, s_ref_lon, &yll, &xll);

                        BBObj.SetMin(xll, yll);
                        BBObj.SetMax(xll, yll);

                    }
                    else
                    {
                        Primitive_type = GEO_POINT;

                        my_fgets(buf, MAX_LINE, *pfpx);
                        int wkb_len = atoi(buf+2);
                        pfpx->Read(buf,  wkb_len);

                        npt = *((int *)(buf + 5));

                        geoPtz = (double *)malloc(npt * 3 * sizeof(double));
                        geoPtMulti = (double *)malloc(npt * 2 * sizeof(double));

                        double *pdd = geoPtz;
                        double *pdl = geoPtMulti;

                        float *pfs = (float *)(buf + 9);                 // start of data
                        for(int ip=0 ; ip<npt ; ip++)
                        {
                            float easting, northing;
                            easting = *pfs++;
                            northing = *pfs++;
                            float depth = *pfs++;

                            *pdd++ = easting;
                            *pdd++ = northing;
                            *pdd++ = depth;

                        //  Convert point from SM to lat/lon for later use in decomposed bboxes
                            double xll, yll;
                            fromSM(easting, northing, s_ref_lat, s_ref_lon, &yll, &xll);

                            *pdl++ = xll;
                            *pdl++ = yll;
                        }

                        // Capture bbox limits recorded in SENC record as lon/lat
                        float xmax = *pfs++;
                        float xmin = *pfs++;
                        float ymax = *pfs++;
                        float ymin = *pfs;

                        BBObj.SetMin(xmin, ymin);
                        BBObj.SetMax(xmax, ymax);

                    }
                    break;
                }

                case 2:                                                // linestring
                {
                    Primitive_type = GEO_LINE;


                    my_fgets(buf, MAX_LINE, *pfpx);
                    int sb_len = atoi(buf+2);

                    unsigned char *buft = (unsigned char *)malloc(sb_len);
                    pfpx->Read(buft,  sb_len);

                    npt = *((int *)(buft + 5));

                    geoPt = (pt*)malloc((npt) * sizeof(pt));
                    pt *ppt = geoPt;
                    float *pf = (float *)(buft + 9);

                        // Capture SM points
                    for(int ip = 0 ; ip < npt ; ip++)
                    {
                        ppt->x = *pf++;
                        ppt->y = *pf++;
                        ppt++;
                    }

                    // Capture bbox limits recorded as lon/lat
                    float xmax = *pf++;
                    float xmin = *pf++;
                    float ymax = *pf++;
                    float ymin = *pf;

                    delete buft;

                    // set s57obj bbox as lat/lon
                    BBObj.SetMin(xmin, ymin);
                    BBObj.SetMax(xmax, ymax);

                    //  and declare x/y of the object to be average east/north of all points
                    double e1, e2, n1, n2;
                    toSM(ymax, xmax, s_ref_lat, s_ref_lon, &e1, &n1);
                    toSM(ymin, xmin, s_ref_lat, s_ref_lon, &e2, &n2);

                    x = (e1 + e2) / 2.;
                    y = (n1 + n2) / 2.;

                   break;
                }

                case 3:                                                                 // area as polygon
                {
// Debug hook
//       if(!strncmp(obj->FeatureName, "DEPCNT", 6))
//           int ffl = 4;
//        if(FEIndex == 1226)
//            int rrt = 5;
                 Primitive_type = GEO_AREA;

                    int ll = strlen(buf);
                    if(ll > llmax)
                        llmax = ll;

                    if(!strncmp(buf, "  POLYTESSGEO", 13))
                    {
                        int nrecl;
                        sscanf(buf, "  POLYTESSGEO %d", &nrecl);

                        if (nrecl)
                        {
                            unsigned char *polybuf = (unsigned char *)malloc(nrecl + 1);
                            pfpx->Read(polybuf,  nrecl);
                            polybuf[nrecl] = 0;                     // endit
                            PolyTessGeo *ppg = new PolyTessGeo(polybuf, nrecl, FEIndex);
                            free(polybuf);

                            pPolyTessGeo = ppg;

                            //  Set the s57obj bounding box as lat/lon
                            BBObj.SetMin(ppg->Get_xmin(), ppg->Get_ymin());
                            BBObj.SetMax(ppg->Get_xmax(), ppg->Get_ymax());

                            //  and declare x/y of the object to be average east/north of all points
                            double e1, e2, n1, n2;
                            toSM(ppg->Get_ymax(), ppg->Get_xmax(), s_ref_lat, s_ref_lon, &e1, &n1);
                            toSM(ppg->Get_ymin(), ppg->Get_xmin(), s_ref_lat, s_ref_lon, &e2, &n2);

                            x = (e1 + e2) / 2.;
                            y = (n1 + n2) / 2.;

                        }
                    }

                    break;
                }
            }       //switch




            if(prim > 0)
            {
               Index = FEIndex;
            }
        }               //OGRF
    }                       //while(!dun)

    free( buf );
    free(hdr_buf);

}

//-------------------------------------------------------------------------------------------
//      Attributes in SENC file may not be needed, and can be safely ignored when creating S57Obj
//      Look at a buffer, and return true or false according to a (default) definition
//-------------------------------------------------------------------------------------------

bool S57Obj::IsUsefulAttribute(char *buf)
{

    if(!strncmp(buf, "HDRLEN", 6))
        return false;

//      Dump the first 8 standard attributes
    /* -------------------------------------------------------------------- */
    /*      RCID                                                            */
    /* -------------------------------------------------------------------- */
    if(!strncmp(buf+2, "RCID", 4))
        return false;

    /* -------------------------------------------------------------------- */
    /*      LNAM                                                            */
    /* -------------------------------------------------------------------- */
    if(!strncmp(buf+2, "LNAM", 4))
        return false;

    /* -------------------------------------------------------------------- */
    /*      PRIM                                                            */
    /* -------------------------------------------------------------------- */
    else if(!strncmp(buf+2, "PRIM", 4))
        return false;

    /* -------------------------------------------------------------------- */
    /*      SORDAT                                                          */
    /* -------------------------------------------------------------------- */
    else if(!strncmp(buf+2, "SORDAT", 6))
        return false;

    /* -------------------------------------------------------------------- */
    /*      SORIND                                                          */
    /* -------------------------------------------------------------------- */
    else if(!strncmp(buf+2, "SORIND", 6))
        return false;

    //      All others are "Useful"
    else
        return true;

#if (0)
    /* -------------------------------------------------------------------- */
    /*      GRUP                                                            */
    /* -------------------------------------------------------------------- */
    else if(!strncmp(buf, "  GRUP", 6))
        return false;

    /* -------------------------------------------------------------------- */
    /*      OBJL                                                            */
    /* -------------------------------------------------------------------- */
    else if(!strncmp(buf, "  OBJL", 6))
        return false;

    /* -------------------------------------------------------------------- */
    /*      RVER                                                            */
    /* -------------------------------------------------------------------- */
    else if(!strncmp(buf, "  RVER", 6))
        return false;

    /* -------------------------------------------------------------------- */
    /*      AGEN                                                            */
    /* -------------------------------------------------------------------- */
    else if(!strncmp(buf, "  AGEN", 6))
        return false;

    /* -------------------------------------------------------------------- */
    /*      FIDN                                                            */
    /* -------------------------------------------------------------------- */
    else if(!strncmp(buf, "  FIDN", 6))
        return false;

    /* -------------------------------------------------------------------- */
    /*      FIDS                                                            */
    /* -------------------------------------------------------------------- */
    else if(!strncmp(buf, "  FIDS", 6))
        return false;

//      UnPresent data
    else if(strstr(buf, "(null)"))
        return false;

    else
        return true;
#endif
}

//------------------------------------------------------------------------------
//      Local version of fgets for Binary Mode (SENC) file
//------------------------------------------------------------------------------
 int S57Obj::my_fgets( char *buf, int buf_len_max, wxBufferedInputStream& ifs )

{
    char        chNext;
    int         nLineLen = 0;
    char            *lbuf;

    lbuf = buf;


    while( !ifs.Eof() && nLineLen < buf_len_max )
    {
        chNext = (char)ifs.GetC();

        /* each CR/LF (or LF/CR) as if just "CR" */
        if( chNext == 10 || chNext == 13 )
        {
            chNext = '\n';
        }

        *lbuf = chNext; lbuf++, nLineLen++;

        if( chNext == '\n' )
        {
            *lbuf = '\0';
            return nLineLen;
        }
    }

    *(lbuf) = '\0';

    return nLineLen;
}

//------------------------------------------------------------------------------
//      Local version of bufgetl for Binary Mode (SENC) file
//------------------------------------------------------------------------------
 int S57Obj::my_bufgetl( char *ib_read, char *ib_end, char *buf, int buf_len_max )
{
    char        chNext;
    int         nLineLen = 0;
    char        *lbuf;
    char        *ibr = ib_read;

    lbuf = buf;


    while( (nLineLen < buf_len_max) && (ibr < ib_end))
    {
        chNext = *ibr++;

        /* each CR/LF (or LF/CR) as if just "CR" */
        if( chNext == 10 || chNext == 13 )
            chNext = '\n';

        *lbuf++ = chNext;
        nLineLen++;

        if( chNext == '\n' )
        {
            *lbuf = '\0';
            return nLineLen;
        }
    }

    *(lbuf) = '\0';
    return nLineLen;
}




//----------------------------------------------------------------------------------
//      s57chart Implementation
//----------------------------------------------------------------------------------

s57chart::s57chart()
{

    ChartType = CHART_TYPE_S57;

    for(int i=0 ; i<PRIO_NUM ; i++)
            for(int j=0 ; j<LUPNAME_NUM ; j++)
                    razRules[i][j] = NULL;

    NativeScale = 1.0;                              // Will be fetched during Init()

    pDIB = NULL;

    m_pFullPath = NULL;

    pFloatingATONArray = NULL;
    pRigidATONArray = NULL;

    tmpup_array = NULL;
    m_pcsv_locn = new wxString(*g_pcsv_locn);

    pDepthUnits->Append(_T("METERS"));
    m_depth_unit_id = DEPTH_UNIT_METERS;

    bGLUWarningSent = false;

    m_pENCDS = NULL;

    m_nvaldco = 0;
    m_nvaldco_alloc = 5;
    m_pvaldco_array = (double *)calloc(m_nvaldco_alloc, sizeof(double));

}

s57chart::~s57chart()
{

    FreeObjectsAndRules();

    delete pDIB;

//    delete pFullPath;

    delete pFloatingATONArray;
    delete pRigidATONArray;

    delete m_pcsv_locn;

    delete m_pENCDS;

    free( m_pvaldco_array );
}



void s57chart::GetValidCanvasRegion(const ViewPort& VPoint, wxRegion *pValidRegion)
{
        int rxl, rxr;
        rxl = 0;
        rxr = VPoint.pix_width;

        int ryb, ryt;
        ryt = 0;
        ryb = VPoint.pix_height;

        pValidRegion->Clear();
        pValidRegion->Union(rxl, ryt, rxr - rxl, ryb - ryt);

}

void s57chart::SetColorScheme(ColorScheme cs, bool bApplyImmediate)
{
    //  Here we convert (subjectively) the Global ColorScheme
    //  to an appropriate S52 Col_Scheme_t index.

    switch(cs)
    {
        case GLOBAL_COLOR_SCHEME_DAY:
            m_S52_color_index = S52_DAY_BRIGHT;
            break;
        case GLOBAL_COLOR_SCHEME_DUSK:
            m_S52_color_index = S52_DUSK;
            break;
        case GLOBAL_COLOR_SCHEME_NIGHT:
            m_S52_color_index = S52_NIGHT;
            break;
        default:
            m_S52_color_index = S52_DAY_BRIGHT;
            break;
    }

    m_global_color_scheme = cs;

    if(bApplyImmediate)
        InvalidateCache();
}

void s57chart::GetChartExtent(Extent *pext)
{
    *pext = FullExtent;
}


void s57chart::FreeObjectsAndRules()
{
//      Delete the created ObjRazRules, including the S57Objs
    ObjRazRules *top;
    ObjRazRules *nxx;
    for (int i=0; i<PRIO_NUM; ++i)
    {
        for(int j=0 ; j<LUPNAME_NUM ; j++)
        {

            top = razRules[i][j];
            while ( top != NULL)
            {
                top->obj->nRef--;
                if(0 == top->obj->nRef)
                    delete top->obj;

                nxx  = top->next;
                free(top);
                top = nxx;
            }
        }
    }
 }


//-----------------------------------------------------------------------
//              Pixel to Lat/Long Conversion helpers
//-----------------------------------------------------------------------

void s57chart::GetPointPix(ObjRazRules *rzRules, float north, float east, wxPoint *r)
{
    r->x = (int)round(((east - easting_vp_center) * view_scale_ppm) + x_vp_center);
    r->y = (int)round(y_vp_center - ((north - northing_vp_center) * view_scale_ppm));
}

void s57chart::GetPixPoint(int pixx, int pixy, double *plat, double *plon, ViewPort *vpt)
{
     //    Use Mercator estimator
    int dx = pixx - (vpt->pix_width / 2);
    int dy = (vpt->pix_height / 2) - pixy;

    double xp = (dx * cos(vpt->skew)) - (dy * sin(vpt->skew));
    double yp = (dy * cos(vpt->skew)) + (dx * sin(vpt->skew));

    double d_east = xp / vpt->view_scale_ppm;
    double d_north = yp / vpt->view_scale_ppm;

    double slat, slon;
    fromSM(d_east, d_north, vpt->clat, vpt->clon, &slat, &slon);

    *plat = slat;
    *plon = slon;

}

//-----------------------------------------------------------------------
//              Calculate and Set ViewPoint Constants
//-----------------------------------------------------------------------

void s57chart::SetVPParms(ViewPort *vpt)
{
    //  Set up local SM rendering constants
    x_vp_center = vpt->pix_width / 2;
    y_vp_center = vpt->pix_height / 2;
    view_scale_ppm = vpt->view_scale_ppm;

    easting_vp_center = vpt->c_east;
    northing_vp_center = vpt->c_north;
}


ThumbData *s57chart::GetThumbData(int tnx, int tny, float lat, float lon)
{
    //  Plot the passed lat/lon at the thumbnail bitmap scale
    //  Using simple linear algorithm.
        if( pThumbData->pDIBThumb)
        {
                float lat_top =   FullExtent.NLAT;
                float lat_bot =   FullExtent.SLAT;
                float lon_left =  FullExtent.WLON;
                float lon_right = FullExtent.ELON;

                // Build the scale factors just as the thumbnail was built
                float ext_max = fmax((lat_top - lat_bot), (lon_right - lon_left));

                float thumb_view_scale_ppm = (S57_THUMB_SIZE/ ext_max) / (1852 * 60);
                double east, north;
                toSM(lat, lon, (lat_top + lat_bot) / 2., (lon_left + lon_right) / 2., &east, &north);

                pThumbData->ShipX = pThumbData->pDIBThumb->GetWidth()  / 2 + (int)(east  * thumb_view_scale_ppm);
                pThumbData->ShipY = pThumbData->pDIBThumb->GetHeight() / 2 - (int)(north * thumb_view_scale_ppm);

        }
        else
        {
                pThumbData->ShipX = 0;
                pThumbData->ShipY = 0;
        }

        return pThumbData;
}

bool s57chart::UpdateThumbData(float lat, float lon)
{
    //  Plot the passed lat/lon at the thumbnail bitmap scale
    //  Using simple linear algorithm.
    int test_x, test_y;
    if( pThumbData->pDIBThumb)
    {
        float lat_top =   FullExtent.NLAT;
        float lat_bot =   FullExtent.SLAT;
        float lon_left =  FullExtent.WLON;
        float lon_right = FullExtent.ELON;

                // Build the scale factors just as the thumbnail was built
        float ext_max = fmax((lat_top - lat_bot), (lon_right - lon_left));

        float thumb_view_scale_ppm = (S57_THUMB_SIZE/ ext_max) / (1852 * 60);
        double east, north;
        toSM(lat, lon, (lat_top + lat_bot) / 2., (lon_left + lon_right) / 2., &east, &north);

        test_x = pThumbData->pDIBThumb->GetWidth()  / 2 + (int)(east  * thumb_view_scale_ppm);
        test_y = pThumbData->pDIBThumb->GetHeight() / 2 - (int)(north * thumb_view_scale_ppm);

    }
    else
    {
        test_x = 0;
        test_y = 0;
    }

    if((test_x != pThumbData->ShipX) || (test_y != pThumbData->ShipY))
    {
        pThumbData->ShipX = test_x;
        pThumbData->ShipY = test_y;
        return TRUE;
    }
    else
        return FALSE;
}



void s57chart::SetFullExtent(Extent& ext)
{
  FullExtent.NLAT = ext.NLAT;
  FullExtent.SLAT = ext.SLAT;
  FullExtent.WLON = ext.WLON;
  FullExtent.ELON = ext.ELON;
}

void s57chart::RenderViewOnDC(wxMemoryDC& dc, ViewPort& VPoint, ScaleTypeEnum scale_type)
{
    ps52plib->SetColorScheme(m_S52_color_index);

    DoRenderViewOnDC(dc, VPoint, DC_RENDER_ONLY);
}



void s57chart::DoRenderViewOnDC(wxMemoryDC& dc, ViewPort& VPoint, RenderTypeEnum option)
{
    wxPoint rul, rlr;

//        int bpp = BPP;          // use global value

    bool bNewVP = false;


        //Todo Add a check on last_vp.bValid
    if(VPoint.view_scale_ppm != last_vp.view_scale_ppm)
    {
        bNewVP = true;
        delete pDIB;
        pDIB = NULL;
    }

//      Calculate the desired rectangle in the last cached image space
    double easting_ul, northing_ul;
    double easting_lr, northing_lr;

    easting_ul =  VPoint.c_east -  ((VPoint.pix_width  / 2) / view_scale_ppm);
    northing_ul = VPoint.c_north + ((VPoint.pix_height / 2) / view_scale_ppm);
    easting_lr = easting_ul + (VPoint.pix_width / view_scale_ppm);
    northing_lr = northing_ul - (VPoint.pix_height / view_scale_ppm);

    prev_easting_ul =  last_vp.c_east -  ((last_vp.pix_width  / 2) / view_scale_ppm);
    prev_northing_ul = last_vp.c_north + ((last_vp.pix_height / 2) / view_scale_ppm);
    prev_easting_lr = easting_ul + (last_vp.pix_width / view_scale_ppm);
    prev_northing_lr = northing_ul - (last_vp.pix_height / view_scale_ppm);


    rul.x = (int)round((easting_ul - prev_easting_ul) * view_scale_ppm);
    rul.y = (int)round((prev_northing_ul - northing_ul) * view_scale_ppm);

    rlr.x = (int)round((easting_lr - prev_easting_ul) * view_scale_ppm);
    rlr.y = (int)round((prev_northing_ul - northing_lr) * view_scale_ppm);

    if((rul.x != 0) || (rul.y != 0))
        bNewVP = true;

    //      Using regions, calculate re-usable area of pDIB

    wxRegion rgn_last(0, 0, VPoint.pix_width, VPoint.pix_height);
    wxRegion rgn_new(rul.x, rul.y, rlr.x - rul.x, rlr.y - rul.y);
    rgn_last.Intersect(rgn_new);            // intersection is reusable portion


    if(bNewVP && (NULL != pDIB) && !rgn_last.IsEmpty())
    {

        int xu, yu, wu, hu;
        rgn_last.GetBox(xu, yu, wu, hu);

        int desx = 0;
        int desy = 0;
        int srcx = xu;
        int srcy = yu;

        if(rul.x < 0)
        {
            srcx = 0;
            desx = -rul.x;
        }
        if(rul.y < 0)
        {
            srcy = 0;
            desy = -rul.y;
        }

        ocpnMemDC dc_last;
        pDIB->SelectIntoDC(dc_last);

        ocpnMemDC dc_new;
        PixelCache *pDIBNew = new PixelCache(VPoint.pix_width, VPoint.pix_height, BPP);
        pDIBNew->SelectIntoDC(dc_new);

//                    printf("blit %d %d %d %d %d %d\n",desx, desy, wu, hu,  srcx, srcy);
        dc_new.Blit(desx, desy, wu, hu, (wxDC *)&dc_last, srcx, srcy, wxCOPY);

        dc_new.SelectObject(wxNullBitmap);
        dc_last.SelectObject(wxNullBitmap);

        delete pDIB;
        pDIB = pDIBNew;

//              OK, now have the re-useable section in place
//              Next, build the new sections

        pDIB->SelectIntoDC(dc);

        wxRegion rgn_delta(0, 0, VPoint.pix_width, VPoint.pix_height);
        wxRegion rgn_reused(desx, desy, wu, hu);
        rgn_delta.Subtract(rgn_reused);

        wxRegionIterator upd(rgn_delta); // get the update rect list
        while (upd)
        {
            wxRect rect = upd.GetRect();


//      Build temp ViewPort on this region

            ViewPort temp_vp = VPoint;

            double temp_northing_ul = prev_northing_ul - (rul.y / view_scale_ppm) - (rect.y / view_scale_ppm);
            double temp_easting_ul = prev_easting_ul + (rul.x / view_scale_ppm) + (rect.x / view_scale_ppm);
            fromSM(temp_easting_ul, temp_northing_ul, ref_lat, ref_lon, &temp_vp.lat_top, &temp_vp.lon_left);

            double temp_northing_lr = temp_northing_ul - (rect.height / view_scale_ppm);
            double temp_easting_lr = temp_easting_ul + (rect.width / view_scale_ppm);
            fromSM(temp_easting_lr, temp_northing_lr, ref_lat, ref_lon, &temp_vp.lat_bot, &temp_vp.lon_right);

            temp_vp.vpBBox.SetMin(temp_vp.lon_left, temp_vp.lat_bot);
            temp_vp.vpBBox.SetMax(temp_vp.lon_right, temp_vp.lat_top);

            //      Allow some slop in the viewport
            temp_vp.vpBBox.EnLarge(temp_vp.vpBBox.GetWidth() * .05);

//      And Render it new piece on the target dc
//     printf("Reuse, rendering %d %d %d %d \n", rect.x, rect.y, rect.width, rect.height);
            DCRenderRect(dc, temp_vp, &rect);

            upd ++ ;
        }

        dc.SelectObject(wxNullBitmap);
    }

    else if(bNewVP || (NULL == pDIB))
    {
        delete pDIB;
        pDIB = new PixelCache(VPoint.pix_width, VPoint.pix_height, BPP);     // destination
//    pDIB->SelectIntoDC(dc);

        wxRect full_rect(0, 0,VPoint.pix_width, VPoint.pix_height);
        pDIB->SelectIntoDC(dc);
        DCRenderRect(dc, VPoint, &full_rect);

        dc.SelectObject(wxNullBitmap);
    }

//      Update last_vp to reflect the current cached bitmap
        last_vp = VPoint;

    pDIB->SelectIntoDC(dc);

}



int s57chart::DCRenderRect(wxMemoryDC& dcinput, ViewPort& vp, wxRect* rect)
{

    int i;
    ObjRazRules *top;
    ObjRazRules *crnt;

    render_canvas_parms pb_spec;

//      Get some heap memory for the area renderer

      pb_spec.depth = BPP;                              // set the depth

        if(rect)
        {
                pb_spec.pb_pitch = ((rect->width * pb_spec.depth / 8 ));
                pb_spec.lclip = rect->x;
                pb_spec.rclip = rect->x + rect->width - 1;
                pb_spec.pix_buff = (unsigned char *)malloc(rect->height * pb_spec.pb_pitch);
                if(NULL == pb_spec.pix_buff)
                    wxLogMessage(_T("PixBuf NULL 1"));

                // Preset background
                memset(pb_spec.pix_buff, 0,rect->height * pb_spec.pb_pitch);
                pb_spec.width = rect->width;
                pb_spec.height = rect->height;
                pb_spec.x = rect->x;
                pb_spec.y = rect->y;
        }
        else
        {
                pb_spec.pb_pitch = ((vp.pix_width * pb_spec.depth / 8 )) ;
                pb_spec.lclip = 0;
                pb_spec.rclip = vp.pix_width-1;
                pb_spec.pix_buff = (unsigned char *)malloc(vp.pix_height * pb_spec.pb_pitch);
                // Preset background
                memset(pb_spec.pix_buff, 0,vp.pix_height * pb_spec.pb_pitch);
                pb_spec.width = vp.pix_width;
                pb_spec.height  = vp.pix_height;
                pb_spec.x = 0;
                pb_spec.y = 0;
        }

//      Render the areas quickly
    for (i=0; i<PRIO_NUM; ++i)
    {
                top = razRules[i][4];           // Area Symbolized Boundaries
                while ( top != NULL)
                {
                        crnt = top;
                        top  = top->next;               // next object
                        ps52plib->RenderArea(&dcinput, crnt, &vp, &pb_spec);
                }

                top = razRules[i][3];           // Area Plain Boundaries
                while ( top != NULL)
                {
                        crnt = top;
                        top  = top->next;               // next object
                        ps52plib->RenderArea(&dcinput, crnt, &vp, &pb_spec);
                }
    }



//      Convert the Private render canvas into a bitmap
#ifdef ocpnUSE_ocpnBitmap
        ocpnBitmap *pREN = new ocpnBitmap(pb_spec.pix_buff,
                                                pb_spec.width, pb_spec.height, pb_spec.depth);
#else
        wxImage *prender_image = new wxImage(pb_spec.width, pb_spec.height, false);
        prender_image->SetData((unsigned char*)pb_spec.pix_buff);
        wxBitmap *pREN = new wxBitmap(*prender_image);

#endif

//      Map it into a temporary DC
        wxMemoryDC dc_ren;
        dc_ren.SelectObject(*pREN);

//      Blit it onto the target dc
        dcinput.Blit(pb_spec.x, pb_spec.y, pb_spec.width, pb_spec.height, (wxDC *)&dc_ren, 0,0);


//      And clean up the mess
        dc_ren.SelectObject(wxNullBitmap);


#ifdef ocpnUSE_ocpnBitmap
        free(pb_spec.pix_buff);
#else
        delete prender_image;           // the image owns the data
                                        // and so will free it in due course
#endif

        delete pREN;



//      Render the rest of the objects/primitives

        DCRenderLPB(dcinput, vp, rect);

        return 1;
}

bool s57chart::DCRenderLPB(wxMemoryDC& dcinput, ViewPort& vp, wxRect* rect)
{
    int i;
    ObjRazRules *top;
    ObjRazRules *crnt;


    for (i=0; i<PRIO_NUM; ++i)
    {
//      Set up a Clipper for Lines
        wxDCClipper *pdcc = NULL;
        if(rect)
        {
            wxRect nr = *rect;
 //         pdcc = new wxDCClipper(dcinput, nr);
        }

        top = razRules[i][2];           //LINES
        while ( top != NULL)
        {
            ObjRazRules *crnt = top;
            top  = top->next;
            ps52plib->_draw(&dcinput, crnt, &vp);


        }

        if(ps52plib->m_nSymbolStyle == SIMPLIFIED)
        {
            top = razRules[i][0];           //SIMPLIFIED Points
            while ( top != NULL)
            {
                crnt = top;
                top  = top->next;
                ps52plib->_draw(&dcinput, crnt, &vp);

            }
        }
        else
        {
            top = razRules[i][1];           //Paper Chart Points Points
            while ( top != NULL)
            {
                crnt = top;
                top  = top->next;
                ps52plib->_draw(&dcinput, crnt, &vp);

            }
        }

        if(ps52plib->m_nBoundaryStyle == SYMBOLIZED_BOUNDARIES)
        {
            top = razRules[i][4];           // Area Symbolized Boundaries
            while ( top != NULL)
            {
                crnt = top;
                top  = top->next;               // next object
                ps52plib->_draw(&dcinput, crnt, &vp);
            }
        }

        else
        {
            top = razRules[i][3];           // Area Plain Boundaries
            while ( top != NULL)
            {
                crnt = top;
                top  = top->next;               // next object
                ps52plib->_draw(&dcinput, crnt, &vp);
            }
        }

        //      Destroy Clipper
        if(pdcc)
            delete pdcc;
    }

/*
        printf("Render Lines                  %ldms\n", stlines.Time());
        printf("Render Simple Points          %ldms\n", stsim_pt.Time());
        printf("Render Paper Points           %ldms\n", stpap_pt.Time());
        printf("Render Symbolized Boundaries  %ldms\n", stasb.Time());
        printf("Render Plain Boundaries       %ldms\n\n", stapb.Time());
*/
        return true;
}


InitReturn s57chart::Init( const wxString& name, ChartInitFlag flags, ColorScheme cs )
{
    m_pFullPath = new wxString(name);

    //  Establish a common reference point for the chart
    ref_lat = (FullExtent.NLAT + FullExtent.SLAT) /2.;
    ref_lon = (FullExtent.WLON + FullExtent.ELON) /2.;

    //  Todo Eventually s_ref_lat/lon goes away.
    s_ref_lat = ref_lat;
    s_ref_lon = ref_lon;

// Look for Thumbnail
        wxFileName ThumbFileNameLook(name);
        ThumbFileNameLook.SetExt(_T("BMP"));

        wxBitmap *pBMP;
        if(ThumbFileNameLook.FileExists())
        {
#ifdef ocpnUSE_ocpnBitmap
                pBMP =  new ocpnBitmap;
#else
                pBMP =  new wxBitmap;
#endif
                pBMP->LoadFile(ThumbFileNameLook.GetFullPath(), wxBITMAP_TYPE_BMP );
                pThumbData->pDIBThumb = pBMP;

        }

        if(flags == THUMB_ONLY)
                return INIT_OK;

 // Really can only Init and use S57 chart if the S52 Presentation Library is OK
    if(!ps52plib->m_bOK)
      return INIT_FAIL_REMOVE;

        int build_ret_val = 1;

        bool bbuild_new_senc = false;

//      Look for S57 file in the target directory
        pS57FileName = new wxFileName(name);
        pS57FileName->SetExt(_T("S57"));

        if(pS57FileName->FileExists())
        {
                wxFile f;
                if(f.Open(pS57FileName->GetFullPath()))
                {
                        if(f.Length() == 0)
                        {
                                f.Close();
                                build_ret_val = BuildS57File( name );
                        }
                        else                                      // file exists, non-zero
                        {                                         // so check for new updates

                                f.Seek(0);
                                wxFileInputStream *pfpx_u = new wxFileInputStream(f);
                                wxBufferedInputStream *pfpx = new wxBufferedInputStream(*pfpx_u);
                                int dun = 0;
                                int last_update = 0;
                                int senc_file_version = 0;
                                int force_make_senc = 0;
                                char buf[256];
                                char *pbuf = buf;

                                while( !dun )
                                {
                                        if(my_fgets(pbuf, 256, *pfpx) == 0)
                                        {
                                                dun = 1;
                                                force_make_senc = 1;
                                                break;
                                        }
                                        else
                                        {
                                                if(!strncmp(pbuf, "OGRF", 4))
                                                {
                                                        dun = 1;
                                                        break;
                                                }
                                                else if(!strncmp(pbuf, "UPDT", 4))
                                                {
                                                        sscanf(pbuf, "UPDT=%i", &last_update);
                                                }
                                                else if(!strncmp(pbuf, "SENC", 4))
                                                {
                                                    sscanf(pbuf, "SENC Version=%i", &senc_file_version);
                                                }
                                        }
                                }

                                delete pfpx;
                                delete pfpx_u;
                                f.Close();
//              Anything to do?

// force_make_senc = 1;
                                wxString DirName(pS57FileName->GetPath((int)wxPATH_GET_SEPARATOR));
                                int most_recent_update_file = GetUpdateFileArray(DirName, NULL);

                                if(last_update != most_recent_update_file)
                                    bbuild_new_senc = true;

                                else if(senc_file_version != CURRENT_SENC_FORMAT_VERSION)
                                    bbuild_new_senc = true;

                                else if(force_make_senc)
                                    bbuild_new_senc = true;

                                if(bbuild_new_senc)
                                      build_ret_val = BuildS57File( name );


                        }
                }
        }

        else                    // SENC file does not exist
        {
                build_ret_val = BuildS57File( name );
                bbuild_new_senc = true;
        }

        if(0 == build_ret_val)
                return INIT_FAIL_RETRY;


        BuildRAZFromS57File( pS57FileName->GetFullPath() );


        //      Check for and if necessary build Thumbnail
        wxFileName ThumbFileName(name);
        ThumbFileName.SetExt(_T("BMP"));

        if(!ThumbFileName.FileExists() || bbuild_new_senc)
        {

                //      Set up a private ViewPort
                ViewPort vp;

                vp.clon = (FullExtent.ELON + FullExtent.WLON) / 2.;
                vp.clat = (FullExtent.NLAT + FullExtent.SLAT) / 2.;
                vp.lat_top =   FullExtent.NLAT;
                vp.lat_bot =   FullExtent.SLAT;
                vp.lon_left =  FullExtent.WLON;
                vp.lon_right = FullExtent.ELON;

                float ext_max = fmax((vp.lat_top - vp.lat_bot), (vp.lon_right - vp.lon_left));

                vp.view_scale_ppm = (S57_THUMB_SIZE/ ext_max) / (1852 * 60);

                vp.pix_height = S57_THUMB_SIZE;
                vp.pix_width  = S57_THUMB_SIZE;

                vp.vpBBox.SetMin(vp.lon_left, vp.lat_bot);
                vp.vpBBox.SetMax(vp.lon_right, vp.lat_top);

                vp.chart_scale = 10000000 - 1;
                vp.bValid = true;
                //Todo this becomes last_vp.bValid = false;
                last_vp.view_scale_ppm = -1;                // cause invalidation of cache
                SetVPParms(&vp);


//      Borrow the OBJLArray temporarily to set the object type visibility for this render
//      First, make a copy for the curent OBJLArray viz settings, setting current value to invisible

                unsigned int OBJLCount = ps52plib->pOBJLArray->GetCount();
                int *psave_viz = new int[OBJLCount];
                int *psvr = psave_viz;
                OBJLElement *pOLE;
                unsigned int iPtr;

                for(iPtr = 0 ; iPtr < OBJLCount ; iPtr++)
                {
                        pOLE = (OBJLElement *)(ps52plib->pOBJLArray->Item(iPtr));
                        *psvr++ = pOLE->nViz;
                        pOLE->nViz = 0;
                }

//      Also, save some other settings
                bool bsavem_bShowSoundgp = ps52plib->m_bShowSoundg;

//      Now, set up what I want for this render
                for(iPtr = 0 ; iPtr < OBJLCount ; iPtr++)
                {
                        pOLE = (OBJLElement *)(ps52plib->pOBJLArray->Item(iPtr));
                        if(!strncmp(pOLE->OBJLName, "LNDARE", 6))
                                pOLE->nViz = 1;
                        if(!strncmp(pOLE->OBJLName, "DEPARE", 6))
                                pOLE->nViz = 1;
                }

                ps52plib->m_bShowSoundg = false;

//      Use display category MARINERS_STANDARD to force use of OBJLArray
                DisCat dsave = ps52plib->m_nDisplayCategory;
                ps52plib->m_nDisplayCategory = MARINERS_STANDARD;

#ifdef ocpnUSE_DIBSECTION
                ocpnMemDC memdc, dc_org;
#else
                wxMemoryDC memdc, dc_org;
#endif

//      set the color scheme
                ps52plib->SetColorScheme(S52_DAY_BRIGHT);

//      Do the render
                DoRenderViewOnDC(memdc, vp, DC_RENDER_ONLY);

//      Release the DIB
                memdc.SelectObject(wxNullBitmap);

//      Restore the plib to previous state
                psvr = psave_viz;
                for(iPtr = 0 ; iPtr < OBJLCount ; iPtr++)
                {
                        pOLE = (OBJLElement *)(ps52plib->pOBJLArray->Item(iPtr));
                        pOLE->nViz = *psvr++;
                }

                ps52plib->m_nDisplayCategory = dsave;
                ps52plib->m_bShowSoundg = bsavem_bShowSoundgp;

//      Reset the color scheme
                ps52plib->SetColorScheme(m_S52_color_index);

                delete psave_viz;

//      Clone pDIB into pThumbData;
                wxBitmap *pBMP;

#ifdef ocpnUSE_ocpnBitmap
                pBMP = new ocpnBitmap((unsigned char *)NULL,
                vp.pix_width, vp.pix_height, BPP);
#else
                pBMP = new wxBitmap(/*NULL,*/
                        vp.pix_width, vp.pix_height, BPP);
#endif
                wxMemoryDC dc_clone;
                dc_clone.SelectObject(*pBMP);

                pDIB->SelectIntoDC(dc_org);

                dc_clone.Blit(0,0,vp.pix_width, vp.pix_height,
                              (wxDC *)&dc_org, 0,0);

                dc_clone.SelectObject(wxNullBitmap);
                dc_org.SelectObject(wxNullBitmap);

//    May Need root to create the Thumbnail file
#ifndef __WXMSW__
                seteuid(file_user_id);
#endif
                pBMP->SaveFile(ThumbFileName.GetFullPath(), wxBITMAP_TYPE_BMP);

//  Update the member thumbdata structure
                wxBitmap *pBMP_NEW;
#ifdef ocpnUSE_ocpnBitmap
                pBMP_NEW =  new ocpnBitmap;
#else
                pBMP_NEW =  new wxBitmap;
#endif
                if(pBMP_NEW->LoadFile(ThumbFileName.GetFullPath(), wxBITMAP_TYPE_BMP ))
                {
                    delete pThumbData;
                    pThumbData = new ThumbData;
                    pThumbData->pDIBThumb = pBMP_NEW;
                }



 //   Return to default user priveleges
#ifndef __WXMSW__
                seteuid(user_user_id);
#endif

        }
        delete pS57FileName;

        m_global_color_scheme = cs;
        SetColorScheme(cs, false);

//    Build array of contour values for later use by conditional symbology

    ObjRazRules *top;
    for (int i=0; i<PRIO_NUM; ++i)
    {
        for(int j=0 ; j<LUPNAME_NUM ; j++)
        {

            top = razRules[i][j];
            while ( top != NULL)
            {
               if(!strncmp(top->obj->FeatureName, "DEPCNT", 6))
               {
                     double valdco = 0.0;
                     if(GetDoubleAttr(top->obj, "VALDCO", valdco))
                     {
                           m_nvaldco++;
                           if(m_nvaldco > m_nvaldco_alloc)
                           {
                                 void *tr = realloc((void *)m_pvaldco_array,m_nvaldco_alloc * 2 * sizeof(double));
                                 m_pvaldco_array = (double *)tr;
                                 m_nvaldco_alloc *= 2;
                           }
                           m_pvaldco_array[m_nvaldco - 1] = valdco;
                     }
               }
               ObjRazRules *nxx  = top->next;
               top = nxx;
            }
        }
    }

    //      And bubble sort it
      bool swap = true;
      int isort;

      while(swap == true)
      {
            swap = false;
            isort = 0;
            while(isort < m_nvaldco - 1)
            {
                  if(m_pvaldco_array[isort+1] < m_pvaldco_array[isort])
                  {
                        double t = m_pvaldco_array[isort];
                        m_pvaldco_array[isort] = m_pvaldco_array[isort+1];
                        m_pvaldco_array[isort+1] = t;
                        swap = true;
                  }
                  isort++;
            }
      }

/*
      for(int j=0 ; j < m_nvaldco ; j++)
      {
            double d = m_pvaldco_array[j];
            int y = 4;
      }
*/

        bReadyToRender = true;

        return INIT_OK;
}

void s57chart::InvalidateCache()
{
        if(pDIB)
        {
                delete pDIB;
                pDIB = NULL;
        }

}


/*    This method returns the smallest chart DEPCNT:VALDCO value which
      is greater than or equal to the specified value
*/
bool s57chart::GetNearestSafeContour(double safe_cnt, double &next_safe_cnt)
{
      int i = 0;
      if(NULL != m_pvaldco_array)
      {
            for(i = 0 ; i < m_nvaldco ; i++)
            {
                  if(m_pvaldco_array[i] >= safe_cnt)
                        break;
            }

            if(i < m_nvaldco)
                  next_safe_cnt = m_pvaldco_array[i];
            else
                  next_safe_cnt = (double)1e6;
            return true;
      }
      else
      {
            next_safe_cnt = (double)1e6;
            return false;
      }
}

/*
--------------------------------------------------------------------------
      Build a list of "associated" DEPARE and DRGARE objects from a given
      object. to be "associated" means to be physically intersecting,
      overlapping, or contained within, depending upon the geometry type
      of the given object.
--------------------------------------------------------------------------
*/


ListOfS57Obj *s57chart::GetAssociatedObjects(S57Obj *obj)
{
      int j;
      int disPrioIdx;

      ListOfS57Obj *pobj_list = new ListOfS57Obj;
      pobj_list->Clear();


      double lat, lon;
      fromSM(obj->x, obj->y, ref_lat, ref_lon, &lat, &lon);

      //    What is the entry object geometry type?

      switch(obj->Primitive_type)
      {
            case GEO_POINT:
                  ObjRazRules *top;
                  disPrioIdx = 1;

                  for(j=0 ; j<LUPNAME_NUM ; j++)
                  {
                        top = razRules[disPrioIdx][j];
                        while ( top != NULL)
                        {
                              if(!strncmp(top->obj->FeatureName, "DEPARE", 6) || !strncmp(top->obj->FeatureName, "DRGARE", 6))
                              {
                                    if(IsPointInObjArea(lat, lon, 0.0, top->obj))
                                    {
                                          pobj_list->Append(top->obj);
                                    }
                              }

                              ObjRazRules *nxx  = top->next;
                              top = nxx;
                        }
                  }

                  break;

            case GEO_LINE:
                  break;

            case GEO_AREA:
                  break;

            default:
                  break;
      }

      return pobj_list;
}


void s57chart::GetChartNameFromTXT(const wxString& FullPath, wxString &Name)
{

      wxFileName fn(FullPath);

      wxString target_name = fn.GetName();
 //     target_name.SetChar(target_name.Len()-1, 'E');
      target_name.RemoveLast();                             // Todo is this OK to use, eg US2EC03 ??

      wxString dir_name = fn.GetPath();

      wxDir dir(dir_name);                                  // The directory containing the file

      wxArrayString *pFileList = new wxArrayString();

      dir.GetAllFiles(fn.GetPath(), pFileList);             // list all the files

      //    Iterate on the file list...

      bool found_name = false;
      wxString name;
      name.Clear();

      for(unsigned int j=0 ; j<pFileList->GetCount() ; j++)
      {
            wxFileName file(pFileList->Item(j));
            if(((file.GetExt()).MakeUpper()) == _T("TXT"))
            {
              //  Look for the line beginning with the name of the .000 file
              wxTextFile text_file(file.GetFullPath());
              text_file.Open();

              wxString str = text_file.GetFirstLine();
              while(!text_file.Eof())
              {
                if(0 == target_name.CmpNoCase(str.Mid(0, target_name.Len())))
                {                                                       // found it
                  wxString tname = str.AfterFirst('-');
                  name = tname.AfterFirst(' ');
                  found_name = true;
                  break;
                }
                else
                {
                  str = text_file.GetNextLine();
                }
              }

              text_file.Close();

              if(found_name)
                break;
            }
      }

      Name = name;

      delete pFileList;

}







//---------------------------------------------------------------------------------
//      S57 Database methods
//---------------------------------------------------------------------------------


//-------------------------------
//
// S57 OBJECT ACCESSOR SECTION
//
//-------------------------------

const char *s57chart::getName(OGRFeature *feature)
{
   return feature->GetDefnRef()->GetName();
}

static int ExtensionCompare(const wxString& first, const wxString& second)
{
        wxFileName fn1(first);
        wxFileName fn2(second);
        wxString ext1(fn1.GetExt());
        wxString ext2(fn2.GetExt());

    return ext1.Cmp(ext2);
}


int s57chart::GetUpdateFileArray(const wxString& DirName, wxArrayString *UpFiles)
{
        wxDir dir(DirName);
        wxString ext;
        wxArrayString *dummy_array;
        int retval = 0;

        if(UpFiles == NULL)
                dummy_array = new wxArrayString;
        else
                dummy_array = UpFiles;

        wxString filename;
        bool cont = dir.GetFirst(&filename);
        while ( cont )
        {
                wxFileName file(filename);
                ext = file.GetExt();
                if(ext.IsNumber() && ext.CmpNoCase(_T("000")))
                {
                        wxString FileToAdd(DirName);
                        FileToAdd.Append(file.GetFullName());
                        dummy_array->Add(FileToAdd);
                }

                cont = dir.GetNext(&filename);
        }

//      Sort the candidates

        dummy_array->Sort(ExtensionCompare);

//      Get the update number of the last in the list
        if(dummy_array->GetCount())
        {
                wxString Last = dummy_array->Last();
                wxFileName fnl(Last);
                ext = fnl.GetExt();
                retval = atoi(ext.mb_str());
        }

        if(UpFiles == NULL)
                delete dummy_array;

        return retval;
}


int s57chart::CountUpdates( const wxString& DirName, wxString &LastUpdateDate)
{

        int retval = 0;

        wxDir dir(DirName);
        wxArrayString *UpFiles = new wxArrayString;
        retval = GetUpdateFileArray(DirName, UpFiles);

        if(retval)
        {
            //      The s57reader of ogr requires that update set be sequentially complete
            //      to perform all the updates.  However, some NOAA ENC distributions are
            //      not complete, as apparently some interim updates have been  withdrawn.
            //      Example:  as of 20 Dec, 2005, the update set for US5MD11M.000 includes
            //      US5MD11M.017, ...018, and ...019.  Updates 001 through 016 are missing.
            //
            //      Workaround.
            //      Create temporary dummy update files to fill out the set before invoking
            //      ogr file open/ingest.  Delete after SENC file create finishes.


            tmpup_array = new wxArrayString;                // save a list of created files for later erase

            for(int iff=1 ; iff < retval ; iff++)
            {
                wxFileName ufile(*m_pFullPath);
                wxString sext;
                sext.Printf(_T("%03d"), iff);
                ufile.SetExt(sext);


                //      Explicit check for a short update file, possibly left over from a crash...
                int flen = 0;
                if(ufile.FileExists())
                {
                    wxFile uf(ufile.GetFullPath());
                    if(uf.IsOpened())
                    {
                        flen = uf.Length();
                        uf.Close();
                    }
                }

                if(ufile.FileExists() && (flen > 25))           // a valid update file
                    continue;

                // Create a dummy ISO8211 file with no real content

                bool bstat;
                DDFModule *dupdate = new DDFModule;
                dupdate->Initialize( '3','L','E','1','0',"!!!",3,4,4 );
                bstat = (bool)dupdate->Create(ufile.GetFullPath().mb_str());
                dupdate->Close();

                if(!bstat)
                {
                    wxString msg(_T("Error creating dummy update file: "));
                    msg.Append(ufile.GetFullPath());
                    wxLogMessage(msg);
                }

                tmpup_array->Add(ufile.GetFullPath());
            }



            //      Extract the date field from the last of the update files
            //      which is by definition a valid, present update file....
            bool bSuccess;
            DDFModule oUpdateModule;

            bSuccess = (bool)oUpdateModule.Open( UpFiles->Last().mb_str(), TRUE );

            if( bSuccess )
            {
//      Get publish/update date
                oUpdateModule.Rewind();
                DDFRecord *pr = oUpdateModule.ReadRecord();                     // Record 0

                int nSuccess;
                char *u = (char *)(pr->GetStringSubfield("DSID", 0, "ISDT", 0, &nSuccess));

                LastUpdateDate = wxString(u, wxConvUTF8);
            }
        }

        delete UpFiles;
        return retval;
}

extern FILE *s_fpdebug;


int s57chart::BuildS57File(const wxString& FullPath)
{
      wxStopWatch sw_build;

      wxString msg0(_T("Building SENC file for "));
      msg0.Append(FullPath);
      wxLogMessage(msg0);


    OGRFeature *objectDef;
    int nProg = 0;

    wxString nice_name;
    GetChartNameFromTXT(FullPath, nice_name);


    wxFileName s57file = wxFileName(FullPath);
    s57file.SetExt(_T("S57"));

    OGREnvelope xt;


    wxString date000;
    int native_scale = 1;


    //      Fetch the Geo Feature Count, or something like it....


    DDFModule *poModule = new DDFModule();
    if(!poModule->Open( FullPath.mb_str() ))
    {
        wxString msg(_T("s57chart::BuildS57File  Unable to open"));
        msg.Append(FullPath);
        wxLogMessage(msg);
        return 0;
    }


    poModule->Rewind();
    DDFRecord *pr = poModule->ReadRecord();                               // Record 0

    int nSuccess;
    nGeoRecords = pr->GetIntSubfield("DSSI", 0, "NOGR", 0, &nSuccess);

//  Use ISDT here, which is the same as UADT for .000 files
    char *u = (char *)(pr->GetStringSubfield("DSID", 0, "ISDT", 0, &nSuccess));
    if(u)
        date000 = wxString(u, wxConvUTF8);

//      Fetch the Native Scale
    for( ; pr != NULL; pr = poModule->ReadRecord() )
    {
        if( pr->FindField( "DSPM" ) != NULL )
        {
            native_scale = pr->GetIntSubfield("DSPM",0,"CSCL",0);
                        break;
        }
    }

    delete poModule;




    wxFileName tfn;
    wxString tmp_file = tfn.CreateTempFileName(_T(""));


    FILE *fps57;
    const char *pp = "wb";
    fps57 = fopen(tmp_file.mb_str(), pp);

    if(fps57 == NULL)
    {
        wxString msg(_T("s57chart::BuildS57File  Unable to create"));
        msg.Append(s57file.GetFullPath());
        wxLogMessage(msg);
        return 0;
    }

    char temp[200];

    fprintf(fps57, "SENC Version= %d\n", CURRENT_SENC_FORMAT_VERSION);

    strncpy(temp, nice_name.mb_str(), 200);
    fprintf(fps57, "NAME=%s\n", temp);

    strncpy(temp, date000.mb_str(), 200);
    fprintf(fps57, "DATE000=%s\n", temp);

    fprintf(fps57, "NOGR=%d\n", nGeoRecords);
    fprintf(fps57, "SCALE=%d\n", native_scale);

    wxString Message = s57file.GetFullPath();
    Message.Append(_T("...Ingesting"));

    wxString Title(_T("OpenCPN S57 SENC File Create..."));
    Title.append(s57file.GetFullPath());

    wxProgressDialog    *SENC_prog;
    SENC_prog = new wxProgressDialog(  Title, Message, nGeoRecords, NULL,
                                       wxPD_AUTO_HIDE | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME |
                                       wxPD_REMAINING_TIME  | wxPD_SMOOTH );

    //      The created size in wxWidgets 2.8 is waaay too large, so....
    wxSize sz = SENC_prog->GetSize();
    sz.x /= 2;
    SENC_prog->SetSize(sz);
    SENC_prog->Centre();


    //      Analyze Updates
    //      The OGR library will apply updates automatically, as specified by the UPDATES=APPLY flag
    //      We need to keep track of the last sequential update applied, to look out for new updates

    int last_applied_update = 0;
    wxString LastUpdateDate = date000;
    last_applied_update = CountUpdates( s57file.GetPath((int)wxPATH_GET_SEPARATOR), LastUpdateDate);

    fprintf(fps57, "UPDT=%d\n", last_applied_update);


    strncpy(temp, LastUpdateDate.mb_str(), 200);
    fprintf(fps57, "DATEUPD=%s\n", temp);

    /*
    //  Here comes the actual ISO8211 file reading
    OGRSFDriver *poDriver;
    OGRDataSource *poDS = OGRSFDriverRegistrar::Open( FullPath.mb_str(), FALSE, &poDriver );
    if( poDS == NULL )
    {
        wxString msg(_T("s57chart::BuildS57File  Unable to open  "));
        msg.Append(FullPath);
        wxLogMessage(msg);
        delete SENC_prog;
        fclose(fps57);

        return 0;
    }
    */

    //  Now that the .000 file with updates is safely ingested, delete any temporary
    //  dummy update files

/*
    if(tmpup_array)
    {
        for(unsigned int iff = 0 ; iff < tmpup_array->GetCount() ; iff++)
           remove(tmpup_array->Item(iff).mb_str());
        delete tmpup_array;
    }
*/

    //      Insert my local error handler to catch OGR errors,
    //      Especially CE_Fatal type errors
    //      Discovered/debugged on US5MD11M.017.  VI 548 geometry deleted
    CPLPushErrorHandler( OpenCPN_OGRErrorHandler );


    bool bcont = true;
    int iObj = 0;
    OGRwkbGeometryType geoType;
    wxString sobj;

    wxStopWatch sw_polygon;
    sw_polygon.Pause();
    wxStopWatch sw_other;
    sw_other.Pause();
    wxStopWatch sw_create_senc_record;
    sw_create_senc_record.Pause();
    wxStopWatch sw_get_next_feature;
    sw_get_next_feature.Pause();

    //////////////Testing
    bcont = SENC_prog->Update(1, _T(""));



    //  Here comes the actual ISO8211 file reading
    OGRS57DataSource *poS57DS = new OGRS57DataSource;
    poS57DS->SetS57Registrar(g_poRegistrar);

    //  Set up the options
    char ** papszReaderOptions = NULL;
    papszReaderOptions = CSLSetNameValue(papszReaderOptions, S57O_LNAM_REFS, "ON" );
    papszReaderOptions = CSLSetNameValue( papszReaderOptions, S57O_UPDATES, "ON");
    poS57DS->SetOptionList(papszReaderOptions);

    wxStopWatch sw_ingest;
    poS57DS->Open(FullPath.mb_str(), TRUE);
    sw_ingest.Pause();

    //  Now that the .000 file with updates is safely ingested, delete any temporary
    //  dummy update files created by CountUpdates()
    if(tmpup_array)
    {
          for(unsigned int iff = 0 ; iff < tmpup_array->GetCount() ; iff++)
                remove(tmpup_array->Item(iff).mb_str());
          delete tmpup_array;
    }


//    Debug
//    FILE *fdebug = VSIFOpen( "\\ocpdebug", "w");
//    s_fpdebug = fdebug;


    S57Reader   *poReader = poS57DS->GetModule(0);

    while(bcont)
    {
            //  Prepare for possible CE_Fatal error in GDAL
            //  Corresponding longjmp() is in the local error handler
          int setjmp_ret = 0;
          setjmp_ret = setjmp(env_ogrf);
          if(setjmp_ret == 1)             //  CE_Fatal happened in GDAL library
                                          //  in the ReadNextFeature() call below.
                                          //  Seems odd, but that's setjmp/longjmp behaviour
                                          //      Discovered/debugged on US5MD11M.017.  VI 548 geometry deleted

          {
//                need to debug thissssssssss
               wxLogMessage(_T("s57chart(): GDAL/OGR Fatal Error caught on Obj #%d"), iObj);
          }

          sw_get_next_feature.Resume();
          objectDef = poReader->ReadNextFeature( );
          sw_get_next_feature.Pause();


          if(objectDef != NULL)
          {

                            iObj++;
//    Debug hook
//                            if(!strncmp(objectDef->GetDefnRef()->GetName(), "M_SREL", 6))
//                                    int ggk = 4;
//                            if(iObj == 1707)
//                                  int ggk = 4;

//  Update the progress dialog
                            sobj = wxString(objectDef->GetDefnRef()->GetName(),  wxConvUTF8);
                            wxString idx;
                            idx.Printf(_T("  %d/%d       "), iObj, nGeoRecords);
                            sobj += idx;

                            nProg = iObj;
                            if(nProg > nGeoRecords - 1)
                                  nProg = nGeoRecords - 1;

                            if(0 == (nProg % 1000))
                                  bcont = SENC_prog->Update(nProg, sobj);


                            geoType = wkbUnknown;
//      This test should not be necessary for real (i.e not C_AGGR) features
//      However... some update files contain errors, and have deleted some
//      geometry without deleting the corresponding feature(s).
//      So, GeometryType becomes Unknown.
//      e.g. US5MD11M.017
//      In this case, all we can do is skip the feature....sigh.

                            if (objectDef->GetGeometryRef() != NULL)
                                  geoType = objectDef->GetGeometryRef()->getGeometryType();


//      Look for polygons to process
                            if(geoType == wkbPolygon)
                            {
                                  int error_code;
                                  PolyTessGeo *ppg = NULL;

                                  OGRPolygon *poly = (OGRPolygon *)(objectDef->GetGeometryRef());

//                                  bcont = SENC_prog->Update(nProg, sobj);

                                  sw_create_senc_record.Resume();
                                  CreateSENCRecord( objectDef, fps57, 0 );
                                  sw_create_senc_record.Pause();

                                  sw_polygon.Resume();
                                  ppg = new PolyTessGeo(poly, true, ref_lat, ref_lon, 0);   //try to use glu library

                                  error_code = ppg->ErrorCode;
                                  if(error_code == ERROR_NO_DLL)
                                  {
                                              if(!bGLUWarningSent)
                                              {
                                                    wxLogMessage(_T("Warning...Could not find glu32.dll, trying internal tess."));
                                                    bGLUWarningSent = true;
                                              }

                                        delete ppg;
                                //  Try with internal tesselator
                                        ppg = new PolyTessGeo(poly, true, ref_lat, ref_lon, 1);
                                        error_code = ppg->ErrorCode;
                                  }


                                  if(error_code)
                                  {
                                              wxLogMessage(_T("Error: S57 SENC Create Error %d"), ppg->ErrorCode);

                                              delete ppg;
                                              delete objectDef;
                                              delete SENC_prog;
                                              fclose(fps57);
                                              delete poS57DS;
                                              CPLPopErrorHandler();
                                              unlink(tmp_file.mb_str());           // delete the temp file....

                                              return 0;                           // soft error return
                                  }
                                  sw_polygon.Pause();

                                  ppg->Write_PolyTriGroup( fps57 );
                                  delete ppg;
                            }

//      n.b  This next line causes skip of C_AGGR features w/o geometry
                            else if( geoType != wkbUnknown )                                // Write only if has wkbGeometry
                            {
                                  sw_other.Resume();
                                  sw_create_senc_record.Resume();

                                  CreateSENCRecord( objectDef, fps57, 1 );

                                  sw_create_senc_record.Pause();
                                  sw_other.Pause();
                            }

                            delete objectDef;


          }
          else
                break;


    }
    delete poS57DS;

//    VSIFClose( s_fpdebug);

    //////////////////
    /*
    for(int iL=0 ; iL < nLayers ; iL++)
    {
        OGRLayer *pLay = poDS->GetLayer(iL);

        pLay->ResetReading();

        iLayObj = 0;

        while (bcont)
        {
            //  Prepare for possible CE_Fatal error in GDAL
            //  Corresponding longjmp() is in the local error handler
            int setjmp_ret = 0;
            setjmp_ret = setjmp(env_ogrf);
            if(setjmp_ret == 1)                 //  CE_Fatal happened in GDAL library
                                                //  in the GetNextFeature() call below.
                                                //  Seems odd, but that's setjmp/longjmp behaviour
            {
                wxString sLay(_T("Unknown"));
                if(iLayObj)
                    sLay = sobj;

                char msg[1000];
                char lay_name[20];
                strncpy(lay_name, sLay.mb_str(), 20);
                sprintf(msg, "s57chart(): GDAL/OGR Fatal Error caught on Obj #%d.\n \
                        Skipping all remaining objects on Layer %s.", iObj, lay_name);

                wxLogMessage(wxString(msg, wxConvUTF8));

                delete objectDef;
                break;                                  // pops out of while(bcont) to next layer
            }
//            sw_get_next_feature.Resume();
            objectDef = pLay->GetNextFeature();
//            sw_get_next_feature.Pause();

            iObj++;
            iLayObj++;

            if(objectDef == NULL)
                break;

//  Debug Hook, can safely go away
//            if(objectDef->GetFID() == 3867)
//                int hhd = 4;

            sobj = wxString(objectDef->GetDefnRef()->GetName(),  wxConvUTF8);
            wxString idx;
            idx.Printf(_T("  %d/%d       "), iObj, nGeoRecords);
            sobj += idx;

//  Update the progress dialog

            nProg = iObj;
            if(nProg > nGeoRecords - 1)
                nProg = nGeoRecords - 1;

            if(0 == (nProg % 100))
                bcont = SENC_prog->Update(nProg, sobj);


            geoType = wkbUnknown;
//      This test should not be necessary for real (i.e not C_AGGR) features
//      However... some update files contain errors, and have deleted some
//      geometry without deleting the corresponding feature(s).
//      So, GeometryType becomes Unknown.
//      e.g. US5MD11M.017
//      In this case, all we can do is skip the feature....sigh.

            if (objectDef->GetGeometryRef() != NULL)
                geoType = objectDef->GetGeometryRef()->getGeometryType();

// Debug
//            if(!strncmp(objectDef->GetDefnRef()->GetName(), "LIGHTS", 6))
//                int ggk = 5;

//      Look for polygons to process
            if(geoType == wkbPolygon)
            {
                int error_code;
                PolyTessGeo *ppg;

                OGRPolygon *poly = (OGRPolygon *)(objectDef->GetGeometryRef());


//              if(1)
                {
                        bcont = SENC_prog->Update(nProg, sobj);

                        sw_create_senc_record.Resume();
                        CreateSENCRecord( objectDef, fps57, 0 );
                        sw_create_senc_record.Pause();

                        sw_polygon.Resume();
                        ppg = new PolyTessGeo(poly, true, ref_lat, ref_lon, 0);   //try to use glu library
                        sw_polygon.Pause();

                        error_code = ppg->ErrorCode;
                        if(error_code == ERROR_NO_DLL)
                        {
                            if(!bGLUWarningSent)
                            {
                                wxLogMessage(_T("Warning...Could not find glu32.dll, trying internal tess."));
                                bGLUWarningSent = true;
                            }

                            delete ppg;
                                //  Try with internal tesselator
                            ppg = new PolyTessGeo(poly, true, ref_lat, ref_lon, 1);
                            error_code = ppg->ErrorCode;
                         }


                        if(error_code)
                        {
                            wxLogMessage(_T("Error: S57 SENC Create Error %d"), ppg->ErrorCode);

                            delete ppg;
                            delete objectDef;
                            delete SENC_prog;
                            fclose(fps57);
                            delete poDS;
                            CPLPopErrorHandler();
                            unlink(tmp_file.mb_str());           // delete the temp file....

                            return 0;                           // soft error return
                        }

                        ppg->Write_PolyTriGroup( fps57 );
                        delete ppg;
                  }
            }

//      n.b  This next line causes skip of C_AGGR features w/o geometry
            else if( geoType != wkbUnknown )                                // Write only if has wkbGeometry
            {
                  sw_other.Resume();
                  sw_create_senc_record.Resume();

                  CreateSENCRecord( objectDef, fps57, 1 );

                  sw_create_senc_record.Pause();
                  sw_other.Pause();
            }

            delete objectDef;


        }           // while bcont

    }               // for

    delete poDS;

    */

    delete SENC_prog;

    fclose(fps57);

    CPLPopErrorHandler();

//    Need root to create the SENC file
#ifndef __WXMSW__
      seteuid(file_user_id);
#endif

    int ret_code = 0;
//      Was the operation cancelled?
    if(!bcont)
    {
        unlink(tmp_file.mb_str());               //      Delete the temp file....
        ret_code = 0;
    }
    else
    {
        remove(s57file.GetFullPath().mb_str());
        unlink(s57file.GetFullPath().mb_str());       //  Delete any existing SENC file....
        int err = rename(tmp_file.mb_str(), s57file.GetFullPath().mb_str()); //   mv temp file to target
        if(err)
        {
            wxString msg(_T("Could not rename temporary SENC file "));
            msg.Append(tmp_file);
            msg.Append(_T(" to "));
            msg.Append(s57file.GetFullPath());
            wxLogMessage(msg);
//            wxString msg1("Could not create SENC file, perhaps permissions not set to read/write?");
//            wxMessageDialog mdlg(this, msg1, wxString("OpenCPN"),wxICON_ERROR  );
//            if(mdlg.ShowModal() == wxID_YES)

            ret_code = 0;
        }
        else
            ret_code = 1;
     }

 //   Return to default user priveleges
#ifndef __WXMSW__
      seteuid(user_user_id);
#endif

      sw_build.Pause();

/*
      wxLogMessage(_T("sw_build: %ld"), sw_build.Time());
      wxLogMessage(_T("sw_ingest: %ld"), sw_ingest.Time());
      wxLogMessage(_T("sw_polygon: %ld"), sw_polygon.Time());
      wxLogMessage(_T("sw_other: %ld"), sw_other.Time());
      wxLogMessage(_T("sw_create_senc_record: %ld"), sw_create_senc_record.Time());
      wxLogMessage(_T("sw_get_next_feature: %ld"), sw_get_next_feature.Time());
*/

      return ret_code;
}

int s57chart::BuildRAZFromS57File( const wxString& FullPath )
{

        int nProg = 0;

        wxString ifs(FullPath);

        wxFileInputStream fpx_u(ifs);
        wxBufferedInputStream fpx(fpx_u);

        int MAX_LINE = 499999;
        char *buf = (char *)malloc(MAX_LINE + 1);

        LUPrec           *LUP;
        LUPname          LUP_Name;

        int     nGeoFeature;

        int object_count = 0;

        OGREnvelope     Envelope;

        int dun = 0;

        hdr_buf = (char *)malloc(1);
        wxProgressDialog    *SENC_prog = NULL;
        int nGeo1000;
        wxString date_000, date_upd;

        if(my_fgets(buf, MAX_LINE, fpx) == 0)
           dun = 1;


        // Create ATON arrays, needed by S52PLIB
        pFloatingATONArray = new wxArrayPtrVoid;
        pRigidATONArray = new wxArrayPtrVoid;



        while(!dun)
        {

                if(my_fgets(buf, MAX_LINE, fpx) == 0)
                {
                        dun = 1;
                        break;
                }

                if(!strncmp(buf, "OGRF", 4))
                {

                    S57Obj *obj = new S57Obj(buf, &fpx);
                    if(obj)
                    {

//      Build/Maintain the ATON floating/rigid arrays
                         if (GEO_POINT == obj->Primitive_type)
                         {

// set floating platform
                            if ((!strncmp(obj->FeatureName, "LITFLT", 6)) ||
                                (!strncmp(obj->FeatureName, "LITVES", 6)) ||
                                (!strncmp(obj->FeatureName, "BOY",    3)))

                                pFloatingATONArray->Add(obj);

// set rigid platform
                            if (!strncmp(obj->FeatureName, "BCN",    3))
                                pRigidATONArray->Add(obj);
                         }



//      This is where Simplified or Paper-Type point features are selected
                         switch(obj->Primitive_type)
                         {
                            case GEO_POINT:
                            case GEO_META:
                            case GEO_PRIM:

                                if(PAPER_CHART == ps52plib->m_nSymbolStyle)
                                    LUP_Name = PAPER_CHART;
                                else
                                    LUP_Name = SIMPLIFIED;

                                break;

                             case GEO_LINE:
                                 LUP_Name = LINES;
                                 break;

                             case GEO_AREA:
                                 if(PLAIN_BOUNDARIES == ps52plib->m_nBoundaryStyle)
                                     LUP_Name = PLAIN_BOUNDARIES;
                                 else
                                     LUP_Name = SYMBOLIZED_BOUNDARIES;

                                 break;
                         }

 // Debug hook
//        if(!strncmp(obj->FeatureName, "CBLSUB", 6))
//            int ffl = 4;
//        if(obj->Index == 1128)
//            int rrt = 5;

                         LUP = ps52plib->S52_LUPLookup(LUP_Name, obj->FeatureName, obj);

                         if(NULL == LUP)
                         {
                             wxLogMessage(_T("Could not find LUP for %s"), obj->FeatureName);
                         }
                         else
                         {
//              Convert LUP to rules set
                            ps52plib->_LUP2rules(LUP, obj);

//              Add linked object/LUP to the working set
                            _insertRules(obj,LUP);
                         }
                    }


                    object_count++;

                    if((object_count % 500) == 0)
                    {
                        nProg = object_count / 500;
                        if(nProg > nGeo1000 - 1)
                                nProg = nGeo1000 - 1;

                        if(SENC_prog)
                            SENC_prog->Update(nProg);
                    }


                    continue;


                }               //OGRF

            else if(!strncmp(buf, "DATEUPD", 7))
            {
                date_upd.Append(wxString(&buf[8], wxConvUTF8).BeforeFirst('\n'));
            }

            else if(!strncmp(buf, "DATE000", 7))
            {
                date_000.Append(wxString(&buf[8], wxConvUTF8).BeforeFirst('\n'));
            }

            else if(!strncmp(buf, "SCALE", 5))
                {
                        int ins;
                        sscanf(buf, "SCALE=%d", &ins);
                        NativeScale = ins;
                }

            else if(!strncmp(buf, "NAME", 4))
            {
                pName->Append(wxString(&buf[5], wxConvUTF8).BeforeFirst('\n'));
            }

            else if(!strncmp(buf, "NOGR", 4))
            {
                 sscanf(buf, "NOGR=%d", &nGeoFeature);

                 nGeo1000 = nGeoFeature / 500;

#ifndef __WXGTK__
                 SENC_prog = new wxProgressDialog(  _T("OpenCPN S57 SENC File Load"), FullPath, nGeo1000, NULL,
                          wxPD_AUTO_HIDE | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME | wxPD_SMOOTH);

    //      The created size in wxWidgets 2.8 is waaay too large, so....
                wxSize sz = SENC_prog->GetSize();
                sz.x /= 2;
                SENC_prog->SetSize(sz);
                SENC_prog->Centre();
#endif
            }
        }                       //while(!dun)


//      fclose(fpx);

        free(buf);

        free(hdr_buf);

        delete SENC_prog;

 //   Decide on pub date to show

        int d000 = atoi((wxString(date_000, wxConvUTF8).Mid(0,4)).mb_str());
        int dupd = atoi((wxString(date_upd, wxConvUTF8).Mid(0,4)).mb_str());

      if(dupd > d000)
          pPubYear->Append(wxString(date_upd, wxConvUTF8).Mid(0,4));
      else
          pPubYear->Append(wxString(date_000, wxConvUTF8).Mid(0,4));

      return 1;
}

//------------------------------------------------------------------------------
//      Local version of fgets for Binary Mode (SENC) file
//------------------------------------------------------------------------------
 int s57chart::my_fgets( char *buf, int buf_len_max, wxBufferedInputStream& ifs )

{
    char        chNext;
    int         nLineLen = 0;
    char            *lbuf;

    lbuf = buf;


    while( !ifs.Eof() && nLineLen < buf_len_max )
    {
        chNext = (char)ifs.GetC();

        /* each CR/LF (or LF/CR) as if just "CR" */
        if( chNext == 10 || chNext == 13 )
        {
            chNext = '\n';
        }

        *lbuf = chNext; lbuf++, nLineLen++;

        if( chNext == '\n' )
        {
            *lbuf = '\0';
            return nLineLen;
        }
    }

    *(lbuf) = '\0';

    return nLineLen;
}



int s57chart::_insertRules(S57Obj *obj, LUPrec *LUP)
{
   ObjRazRules   *rzRules = NULL;
   int                          disPrioIdx = 0;
   int                          LUPtypeIdx = 0;

   if(LUP == NULL){
      printf("SEQuencer:_insertRules(): ERROR no rules to insert!!\n");
      return 0;
   }


   // find display priority index       --talky version
   switch(LUP->DPRI){
      case PRIO_NODATA:         disPrioIdx = 0; break;  // no data fill area pattern
      case PRIO_GROUP1:         disPrioIdx = 1; break;  // S57 group 1 filled areas
      case PRIO_AREA_1:         disPrioIdx = 2; break;  // superimposed areas
      case PRIO_AREA_2:         disPrioIdx = 3; break;  // superimposed areas also water features
      case PRIO_SYMB_POINT:     disPrioIdx = 4; break;  // point symbol also land features
      case PRIO_SYMB_LINE:      disPrioIdx = 5; break;  // line symbol also restricted areas
      case PRIO_SYMB_AREA:      disPrioIdx = 6; break;  // area symbol also traffic areas
      case PRIO_ROUTEING:       disPrioIdx = 7; break;  // routeing lines
      case PRIO_HAZARDS:        disPrioIdx = 8; break;  // hazards
      case PRIO_MARINERS:       disPrioIdx = 9; break;  // VRM & EBL, own ship
      default:
         printf("SEQuencer:_insertRules():ERROR no display priority!!!\n");
   }

   // find look up type index
   switch(LUP->TNAM){
      case SIMPLIFIED:                          LUPtypeIdx = 0; break; // points
      case PAPER_CHART:                         LUPtypeIdx = 1; break; // points
      case LINES:                                       LUPtypeIdx = 2; break; // lines
      case PLAIN_BOUNDARIES:            LUPtypeIdx = 3; break; // areas
      case SYMBOLIZED_BOUNDARIES:       LUPtypeIdx = 4; break; // areas
      default:
         printf("SEQuencer:_insertRules():ERROR no look up type !!!\n");
   }

   // insert rules
   rzRules = (ObjRazRules *)malloc(sizeof(ObjRazRules));
   rzRules->obj   = obj;
   obj->nRef++;                         // Increment reference counter for delete check;
   rzRules->LUP   = LUP;
   rzRules->chart = this;
   rzRules->next  = razRules[disPrioIdx][LUPtypeIdx];
   razRules[disPrioIdx][LUPtypeIdx] = rzRules;

   return 1;
}


//      Traverse the ObjRazRules tree, and fill in
//      any Lups/rules not linked on initial chart load.
//      For example, if chart was loaded with PAPER_CHART symbols,
//      locate and load the equivalent SIMPLIFIED symbology.
//      Likewise for PLAIN/SYMBOLIZED boundaries.
//
//      This method is usually called after a chart display style
//      change via the "Options" dialog, to ensure all symbology is
//      present iff needed.

void s57chart::UpdateLUPs()
{
    ObjRazRules *top;
    ObjRazRules *nxx;
    LUPrec      *LUP;

    for (int i=0; i<PRIO_NUM; ++i)
    {
        //  SIMPLIFIED is set, PAPER_CHART is bare
        if((razRules[i][0]) && (NULL == razRules[i][1]))
        {
            top = razRules[i][0];

            while ( top != NULL)
            {
                LUP = ps52plib->S52_LUPLookup(PAPER_CHART, top->obj->FeatureName, top->obj);
                ps52plib->_LUP2rules(LUP, top->obj);
                _insertRules(top->obj, LUP);

                nxx  = top->next;
                top = nxx;
            }
        }

                //  PAPER_CHART is set, SIMPLIFIED is bare
        if((razRules[i][1]) && (NULL == razRules[i][0]))
        {
            top = razRules[i][1];

            while ( top != NULL)
            {
                LUP = ps52plib->S52_LUPLookup(SIMPLIFIED, top->obj->FeatureName, top->obj);
                ps52plib->_LUP2rules(LUP, top->obj);
                _insertRules(top->obj, LUP);

                nxx  = top->next;
                top = nxx;
            }
        }

                //  PLAIN_BOUNDARIES is set, SYMBOLIZED_BOUNDARIES is bare
        if((razRules[i][3]) && (NULL == razRules[i][4]))
        {
            top = razRules[i][3];

            while ( top != NULL)
            {
                LUP = ps52plib->S52_LUPLookup(SYMBOLIZED_BOUNDARIES, top->obj->FeatureName, top->obj);
                ps52plib->_LUP2rules(LUP, top->obj);
                _insertRules(top->obj, LUP);

                nxx  = top->next;
                top = nxx;
            }
        }

                //  SYMBOLIZED_BOUNDARIES is set, PLAIN_BOUNDARIES is bare
        if((razRules[i][4]) && (NULL == razRules[i][3]))
        {
            top = razRules[i][4];

            while ( top != NULL)
            {
                LUP = ps52plib->S52_LUPLookup(PLAIN_BOUNDARIES, top->obj->FeatureName, top->obj);
                ps52plib->_LUP2rules(LUP, top->obj);
                _insertRules(top->obj, LUP);

                nxx  = top->next;
                top = nxx;
            }
        }

        //  Traverse this priority level again,
        //  clearing any object CS rules and flags,
        //  so that the next render operation will re-evaluate the CS

        for(int j=0 ; j<LUPNAME_NUM ; j++)
        {
            top = razRules[i][j];
            while ( top != NULL)
            {
                top->obj->bCS_Added = 0;

                nxx  = top->next;
                top = nxx;
            }
        }
    }

          //    Clear the dynamically created Conditional Symbology LUP Array
    ps52plib->ClearCNSYLUPArray();

}






void s57chart::CreateSENCRecord( OGRFeature *pFeature, FILE * fpOut, int mode )
{

#define MAX_HDR_LINE    400

        char line[MAX_HDR_LINE + 1];
        wxString sheader;

        fprintf( fpOut, "OGRFeature(%s):%ld\n", pFeature->GetDefnRef()->GetName(),
                  pFeature->GetFID() );

        // DEBUG
//        if(pFeature->GetFID() == 3868)
//          int hhl = 5;

//      In the interests of output file size, DO NOT report fields that are not set.
        for( int iField = 0; iField < pFeature->GetFieldCount(); iField++ )
        {
                if( pFeature->IsFieldSet( iField ) )
                {
                        if( (iField == 1) || (iField > 7))
                        {
                                OGRFieldDefn *poFDefn = pFeature->GetDefnRef()->GetFieldDefn(iField);

                                const char *pType = OGRFieldDefn::GetFieldTypeName(poFDefn->GetType()) ;

                                snprintf( line, MAX_HDR_LINE - 2, "  %s (%c) = %s",
                                         poFDefn->GetNameRef(),
                                         *pType,
                                         pFeature->GetFieldAsString( iField ));

                        sheader += wxString(line, wxConvUTF8);
                        sheader += '\n';
                        }
                }
        }

        OGRGeometry *pGeo = pFeature->GetGeometryRef();


//    Special geometry cases
        if(wkbPoint == pGeo->getGeometryType())
        {
              OGRPoint *pp = (OGRPoint *)pGeo;
              int nqual = pp->getnQual();
              if(10 != nqual)                         // only add attribute if nQual is not "precisely known"
              {
                    snprintf( line, MAX_HDR_LINE - 2, "  %s (%c) = %d","QUALTY", 'I', nqual);
                    sheader += wxString(line, wxConvUTF8);
                    sheader += '\n';
              }

        }

        if(mode == 1)
        {
            sprintf( line, "  %s\n", pGeo->getGeometryName());
            sheader += wxString(line, wxConvUTF8);
        }

        fprintf( fpOut, "HDRLEN=%d\n", sheader.Len());
        fwrite(sheader.mb_str(), 1, sheader.Len(), fpOut);

        if(( pGeo != NULL ) && (mode == 1))
        {
            int wkb_len = pGeo->WkbSize();
            unsigned char *pwkb_buffer = (unsigned char *)malloc(wkb_len);

//  Get the GDAL data representation
            pGeo->exportToWkb(wkbNDR, pwkb_buffer);

    //  Convert to opencpn SENC representation

    //  Set absurd bbox starting limits
            float lonmax = -1000;
            float lonmin = 1000;
            float latmax = -1000;
            float latmin = 1000;

            int i, ip, sb_len;
            float *pdf;
            double *psd;
            unsigned char *ps;
            unsigned char *pd;
            unsigned char *psb_buffer;
            double lat, lon;
            int nPoints;
            wxString msg;

            OGRwkbGeometryType gType = pGeo->getGeometryType();
            switch(gType)
            {
                case wkbLineString:
                    sb_len = ((wkb_len - 9) / 2) + 9 + 16;                // data will be 4 byte float, not double
                                                                          // and bbox limits are tacked on end
                    fprintf( fpOut, "  %d\n", sb_len);


                    psb_buffer = (unsigned char *)malloc(sb_len);
                    pd = psb_buffer;
                    ps = pwkb_buffer;

                    memcpy(pd, ps, 9);                                    // byte order, type, and count

                    ip = *((int *)(ps + 5));                              // point count

                    pd += 9;
                    ps += 9;
                    psd = (double *)ps;
                    pdf = (float *)pd;

                    for(i = 0 ; i < ip ; i++)                           // convert doubles to floats
                    {                                                   // computing bbox as we go

                        float lon = (float)*psd++;
                        float lat = (float)*psd++;

                        //  Calculate SM from chart common reference point
                        double easting, northing;
                        toSM(lat, lon, ref_lat, ref_lon, &easting, &northing);

                        *pdf++ = easting;
                        *pdf++ = northing;

                        lonmax = fmax(lon, lonmax);
                        lonmin = fmin(lon, lonmin);
                        latmax = fmax(lat, latmax);
                        latmin = fmin(lat, latmin);

                    }

                    //      Store the Bounding Box as lat/lon
                    *pdf++ = lonmax;
                    *pdf++ = lonmin;
                    *pdf++ = latmax;
                    *pdf =   latmin;

                    fwrite(psb_buffer, 1, sb_len, fpOut);
                    free(psb_buffer);
                    break;

                case wkbMultiLineString:
                    msg = _T("Warning: Unimplemented SENC wkbMultiLineString record in file ");
                    msg.Append(pS57FileName->GetFullPath());
                    wxLogMessage(msg);

                      wkb_len = pGeo->WkbSize();
                      fprintf( fpOut, "  %d\n", wkb_len);
                      fwrite(pwkb_buffer, 1, wkb_len, fpOut);

                      break;

                case wkbPoint:
                    {
                    int nq_len = 4;                                     // nQual length
//                    int nqual = *(int *)(pwkb_buffer + 5);              // fetch nqual

                    sb_len = ((wkb_len - (5 + nq_len)) / 2) + 5;        // data will be 4 byte float, not double
                                                                        // and skipping nQual

                    fprintf( fpOut, "  %d\n", sb_len);

                    psb_buffer = (unsigned char *)malloc(sb_len);
                    pd = psb_buffer;
                    ps = pwkb_buffer;

                    memcpy(pd, ps, 5);                                 // byte order, type

                    pd += 5;
                    ps += 5 + nq_len;
                    psd = (double *)ps;
                    pdf = (float *)pd;

                    lon = *psd++;                                      // fetch the point
                    lat = *psd;

                    //  Calculate SM from chart common reference point
                    double easting, northing;
                    toSM(lat, lon, ref_lat, ref_lon, &easting, &northing);

                    *pdf++ = easting;
                    *pdf   = northing;

                    //  And write it out
                    fwrite(psb_buffer, 1, sb_len, fpOut);
                    free(psb_buffer);

                    break;
                    }

                case wkbMultiPoint25D:
                    ps = pwkb_buffer;
                    ps += 5;
                    nPoints = *((int *)ps);                     // point count

                    sb_len = (9 + nPoints * 3 * sizeof(float)) + 16;     // GTYPE and count, points as floats
                                                                         // and trailing bbox
                    fprintf( fpOut, "  %d\n", sb_len);

                    psb_buffer = (unsigned char *)malloc(sb_len);
                    pd = psb_buffer;
                    ps = pwkb_buffer;

                    memcpy(pd, ps, 9);                                  // byte order, type, count

                    ps += 9;
                    pd += 9;

                    pdf = (float *)pd;

                    for(ip=0 ; ip < nPoints ; ip++)
                    {

                        // Workaround a bug?? in OGRGeometryCollection
                        // While exporting point geometries serially, OGRPoint->exportToWkb assumes that
                        // if Z is identically 0, then the point must be a 2D point only.
                        // So, the collection Wkb is corrupted with some 3D, and some 2D points.
                        // Workaround:  Get reference to the points serially, and explicitly read X,Y,Z
                        // Ignore the previously read Wkb buffer

                        OGRGeometryCollection *temp_geometry_collection = (OGRGeometryCollection *)pGeo;
                        OGRGeometry *temp_geometry = temp_geometry_collection->getGeometryRef( ip );
                        OGRPoint *pt_geom = (OGRPoint *)temp_geometry;

                        lon = pt_geom->getX();
                        lat = pt_geom->getY();
                        double depth = pt_geom->getZ();

                        /*
                        ps += 5;
                        psd = (double *)ps;

                        lon = *psd++;
                        lat = *psd++;
                        double depth = *psd;
                        */

                        //  Calculate SM from chart common reference point
                        double easting, northing;
                        toSM(lat, lon, ref_lat, ref_lon, &easting, &northing);

                        *pdf++ = easting;
                        *pdf++ = northing;
                        *pdf++ = (float)depth;

//                        ps += 3 * sizeof(double);

                        //  Keep a running calculation of min/max
                        lonmax = fmax(lon, lonmax);
                        lonmin = fmin(lon, lonmin);
                        latmax = fmax(lat, latmax);
                        latmin = fmin(lat, latmin);
                    }

                    //      Store the Bounding Box as lat/lon
                    *pdf++ = lonmax;
                    *pdf++ = lonmin;
                    *pdf++ = latmax;
                    *pdf =   latmin;

                    //  And write it out
                    fwrite(psb_buffer, 1, sb_len, fpOut);
                    free(psb_buffer);


                    break;

                    //      Special case, polygons are handled separately
                case wkbPolygon:
                    break;

                    //      All others
                default:
                    msg = _T("Warning: Unimplemented ogr geotype record in file ");
                    msg.Append(pS57FileName->GetFullPath());
                    wxLogMessage(msg);

                    wkb_len = pGeo->WkbSize();
                      fprintf( fpOut, "  %d\n", wkb_len);
                      fwrite(pwkb_buffer, 1, wkb_len, fpOut);
                      break;
            }       // switch



            free(pwkb_buffer);
            fprintf( fpOut, "\n" );
        }
}


/*
      LUPname     m_nSymbolStyle;
      LUPname     m_nBoundaryStyle;
      bool        m_bOK;

      bool        m_bShowSoundg;
      bool        m_bShowMeta;
      bool        m_bShowS57Text;
      bool        m_bUseSCAMIN;
*/

ArrayOfS57Obj *s57chart::GetObjArrayAtLatLon(float lat, float lon, float select_radius, ViewPort *VPoint)
{

    ArrayOfS57Obj *ret_ptr = new ArrayOfS57Obj;


//    Iterate thru the razRules array, by object/rule type

    ObjRazRules *crnt;
    ObjRazRules *top;

    for (int i=0; i<PRIO_NUM; ++i)
    {
      // Points by type, array indices [0..1]

        int point_type = (ps52plib->m_nSymbolStyle == SIMPLIFIED) ? 0 : 1;
        top = razRules[i][point_type];

        while ( top != NULL)
        {
            crnt = top;
            top  = top->next;

            if(ps52plib->ObjectRenderCheck(crnt, VPoint))
            {
                if(DoesLatLonSelectObject(lat, lon, select_radius, crnt->obj))
                    ret_ptr->Add(crnt->obj);
            }

        }


      // Areas by boundary type, array indices [3..4]

        int area_boundary_type = (ps52plib->m_nBoundaryStyle == PLAIN_BOUNDARIES) ? 3 : 4;
        top = razRules[i][area_boundary_type];           // Area nnn Boundaries
        while ( top != NULL)
        {
            crnt = top;
            top  = top->next;
            if(ps52plib->ObjectRenderCheck(crnt, VPoint))
            {
                if(DoesLatLonSelectObject(lat, lon, select_radius, crnt->obj))
                    ret_ptr->Add(crnt->obj);
            }
        }         // while


      // Finally, lines
          top = razRules[i][2];           // Lines

          while ( top != NULL)
          {
            crnt = top;
            top  = top->next;
            if(ps52plib->ObjectRenderCheck(crnt, VPoint))
            {
                if(DoesLatLonSelectObject(lat, lon, select_radius, crnt->obj))
                ret_ptr->Add(crnt->obj);
            }
          }
      }

      return ret_ptr;
}

bool s57chart::DoesLatLonSelectObject(float lat, float lon, float select_radius, S57Obj *obj)
{
      switch(obj->Primitive_type)
      {
            case  GEO_POINT:
                //  For single Point objects, the integral object bounding box contains the lat/lon of the object,
                //  possibly expanded by text or symbol rendering
                {
                    if(1 == obj->npt)
                    {
                        if(obj->BBObj.PointInBox( lon, lat, select_radius))
                            return true;
                    }
                    //  For MultiPoint objects, make a bounding box from each point's lat/lon
                    //  and check it
                    else
                    {
                        //  Coarse test first
                        if(!obj->BBObj.PointInBox( lon, lat, select_radius))
                            return false;
                        //  Now decomposed soundings, one by one
                        double *pdl = obj->geoPtMulti;
                        for(int ip = 0 ; ip < obj->npt ;  ip++)
                        {
                            double lon_point = *pdl++;
                            double lat_point = *pdl++;
                            wxBoundingBox BB_point(lon_point, lat_point, lon_point, lat_point);
                            if(BB_point.PointInBox( lon, lat, select_radius))
                                return true;
                        }
                    }

                    break;
                }
            case  GEO_AREA:
                {
                    return IsPointInObjArea(lat, lon, select_radius, obj);
                }

            case  GEO_LINE:
            {
                  if(obj->geoPt)
                  {
                        //  Coarse test first
                        if(!obj->BBObj.PointInBox( lon, lat, select_radius))
                              return false;

        //  Polygon geometry is carried in SM coordinates, so...
        //  make the hit test thus.
                        double easting, northing;
                        toSM(lat, lon, ref_lat, ref_lon, &easting, &northing);

                        pt *ppt = obj->geoPt;
                        int npt = obj->npt;
                        float north0 = ppt->y;
                        float east0 = ppt->x;
                        ppt++;

                        for(int ip=1 ; ip<npt ; ip++)
                        {
                              float north = ppt->y;
                              float east = ppt->x;

                              //    A slightly less coarse segment bounding box check
                              if(northing >= (fmin(north, north0) - select_radius))
                                    if(northing <= (fmax(north, north0) + select_radius))
                                          if(easting >= (fmin(east, east0) - select_radius))
                                                if(easting <= (fmax(east, east0) + select_radius))
                                                {
                                                      return true;
                                                }


                              north0 = north;
                              east0 = east;
                              ppt++;
                        }
                  }

                  break;
            }

            case  GEO_META:
            case  GEO_PRIM:

            break;
      }

      return false;
}

wxString *s57chart::CreateObjDescription(const S57Obj& obj)
{
      wxString *ret_str = new wxString;

      char *curr_att;
      int iatt;
      wxString att, value;
      S57attVal *pval;

      switch(obj.Primitive_type)
      {
            case  GEO_POINT:
            case  GEO_AREA:
            case  GEO_LINE:
            {

                  //    Get Name
                      wxString name(obj.FeatureName,  wxConvUTF8);
                      *ret_str << name;
                      *ret_str << _T(" - ");

                  //    Get the object's nice description from s57objectclasses.csv
                  //    using cpl_csv from the gdal library

                  const char *name_desc;
                  if(NULL != m_pcsv_locn)
                  {
                    wxString oc_file(*m_pcsv_locn);
                    oc_file.Append(_T("/s57objectclasses.csv"));
                    name_desc = MyCSVGetField(oc_file.mb_str(),
                                     "Acronym",                  // match field
                                     obj.FeatureName,            // match value
                                     CC_ExactString,
                                     "ObjectClass");             // return field
                  }
                  else
                      name_desc = "";


                  *ret_str << wxString(name_desc,  wxConvUTF8);
                  *ret_str << _T('\n');


                  wxString index;
                  index.Printf(_T("    Feature Index: %d\n"), obj.Index);
                  *ret_str << index;


                  //    Get the Attributes and values

                  char *curr_att0 = (char *)calloc(obj.attList->Len()+1, 1);
                  strncpy(curr_att0, obj.attList->mb_str(), obj.attList->Len());
                  curr_att = curr_att0;

                  iatt = 0;

                  while(*curr_att)
                  {
//    Attribute name
                        att.Clear();
                        while((*curr_att) && (*curr_att != '\037'))
                        {
                              char t = *curr_att++;
                              att.Append(t);
                        }

                        if(*curr_att == '\037')
                              curr_att++;

                        int is = 0;
                        while( is < 8)
                        {
                              *ret_str << _T(' ');
                              is++;
                        }

                        *ret_str << att;

                        is+= att.Len();
                        while( is < 25)
                        {
                              *ret_str << _T(' ');
                              is++;
                        }


// What we need to do...
// Change senc format, instead of (S), (I), etc, use the attribute types fetched from the S57attri...csv file
// This will be like (E), (L), (I), (F)
//  will affect lots of other stuff.  look for S57attVal.valType
// need to do this in creatsencrecord above, and update the senc format.

//    Attribute encoded value
                    value.Clear();

                    pval = obj.attVal->Item(iatt);
                    switch(pval->valType)
                    {
                        case OGR_STR:
                        {
                            if(pval->value)
                            {
                                wxString val_str((char *)(pval->value),  wxConvUTF8);
                                if(val_str.IsNumber())
                                {
                                    int ival = atoi(val_str.mb_str());
                                    if(0 == ival)
                                        value = _T("Unknown");
                                    else
                                    {
                                        wxString *decode_val = GetAttributeDecode(att, ival);
                                        if(decode_val)
                                        {
                                            value = *decode_val;
                                            wxString iv;
                                            iv.Printf(_T("(%d)"), ival);
                                            value.Append(iv);
                                        }
                                        else
                                            value.Printf(_T("(%d)"), ival);
                                        delete decode_val;
                                    }
                                }

                                else if(val_str.IsEmpty())
                                    value = _T("Unknown");

                                else
                                {
                                    value.Clear();
                                    wxString value_increment;
                                    wxStringTokenizer tk(val_str, wxT(","));
                                    int iv = 0;
                                    while ( tk.HasMoreTokens() )
                                    {
                                        wxString token = tk.GetNextToken();
                                        if(token.IsNumber())
                                        {
                                            int ival = atoi(token.mb_str());
                                            wxString *decode_val = GetAttributeDecode(att, ival);
                                            if(decode_val)
                                                value_increment = *decode_val;
                                            else
                                                value_increment.Printf(_T("(%d)"), ival);

                                            delete decode_val;
                                            if(iv)
                                                value_increment.Prepend(wxT(", "));
                                        }
                                        value.Append(value_increment);

                                        iv++;
                                    }

                                    value.Append(_T("("));
                                    value.Append(val_str);
                                    value.Append(_T(")"));
                                }
                            }
                            else
                                value = _T("[NULL VALUE]");

                            break;
                        }

                        case OGR_INT:
                        {
                            int ival = *((int *)pval->value);
                            wxString *decode_val = GetAttributeDecode(att, ival);

                            if(decode_val)
                            {
                                value = *decode_val;
                                wxString iv;
                                iv.Printf(_T("(%d)"), ival);
                                value.Append(iv);
                            }
                            else
                                value.Printf(_T("(%d)"), ival);

                            delete decode_val;
                            break;
                        }
                        case OGR_INT_LST:
                              break;

                        case OGR_REAL:
                        {
/*
                              float dval = *((float *)pval->value);
                              value.Printf(_T("%g"), dval);
*/
                              double dval = *((double *)pval->value);
                              value.Printf(_T("%g"), dval);
                              break;
                        }

                        case OGR_REAL_LST:
                        {
                                break;
                        }
                    }


                        *ret_str << value;

                        *ret_str << _T('\n');
                        iatt++;

                  }             //while *curr_att

                  free(curr_att0);

                  return ret_str;

                  }             //case

//      wxString                *attList;
//      wxArrayOfS57attVal      *attVal;

#if 0
typedef enum _OGRatt_t{
   OGR_INT,
   OGR_INT_LST,
   OGR_REAL,
   OGR_REAL_LST,
   OGR_STR,
}OGRatt_t;

typedef struct _S57attVal{
   void *   value;
   OGRatt_t valType;
}S57attVal;
#endif


            case  GEO_META:
            case  GEO_PRIM:

            break;
      }

      return ret_str;
}

wxString *s57chart::GetAttributeDecode(wxString& att, int ival)
{

    wxString *ret_val = NULL;

    if(NULL == m_pcsv_locn)
        return NULL;

    //  Get the attribute code from the acronym
    const char *att_code;

    wxString file(*m_pcsv_locn);
    file.Append(_T("/s57attributes.csv"));
    att_code = MyCSVGetField(file.mb_str(),
                                  "Acronym",                  // match field
                                  att.mb_str(),               // match value
                                  CC_ExactString,
                                  "Code");             // return field


    // Now, get a nice description from s57expectedinput.csv
    //  This will have to be a 2-d search, using ID field and Code field

    bool more = true;
    wxString ei_file(*m_pcsv_locn);
    ei_file.Append(_T("/s57expectedinput.csv"));

    FILE        *fp;
    const char *pp = "rb";
    fp = VSIFOpen( ei_file.mb_str(), pp );
    if( fp == NULL )
        return NULL;

    while(more)
    {
        char **result = CSVScanLines( fp,
                                     0,                         // first field = attribute Code
                                     att_code,
                                     CC_ExactString );

        if(NULL == result)
        {
            more = false;
            break;
        }
        if(atoi(result[1]) == ival)
        {
            ret_val = new wxString(result[2],  wxConvUTF8);
        }
    }


    VSIFClose(fp);
    return ret_val;
}



//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------


bool s57chart::IsPointInObjArea(float lat, float lon, float select_radius, S57Obj *obj)
{
    bool ret = false;

    if(obj->pPolyTessGeo)
    {

//      Is the point in the PolyGeo Bounding Box?

        if(lon > obj->pPolyTessGeo->Get_xmax())
            return false;
        else if(lon < obj->pPolyTessGeo->Get_xmin())
            return false;
        else if(lat > obj->pPolyTessGeo->Get_ymax())
            return false;
        else if(lat < obj->pPolyTessGeo->Get_ymin())
            return false;


        PolyTriGroup *ppg = obj->pPolyTessGeo->Get_PolyTriGroup_head();

        TriPrim *pTP = ppg->tri_prim_head;

        MyPoint pvert_list[3];

        //  Polygon geometry is carried in SM coordinates, so...
        //  make the hit test thus.
        double easting, northing;
        toSM(lat, lon, ref_lat, ref_lon, &easting, &northing);

        while(pTP)
        {
//  Coarse test
            if(pTP->p_bbox->PointInBox(lon, lat, 0))
            {
                double *p_vertex = pTP->p_vertex;

                switch (pTP->type)
                {
                    case PTG_TRIANGLE_FAN:
                    {
                        for(int it = 0 ; it < pTP->nVert - 2 ; it++)
                        {
                            pvert_list[0].x = p_vertex[0];
                            pvert_list[0].y = p_vertex[1];

                            pvert_list[1].x = p_vertex[(it*2)+2];
                            pvert_list[1].y = p_vertex[(it*2)+3];

                            pvert_list[2].x = p_vertex[(it*2)+4];
                            pvert_list[2].y = p_vertex[(it*2)+5];

                            if(G_PtInPolygon((MyPoint *)pvert_list, 3, easting, northing))
                            {
                                ret = true;
                                break;
                            }
                        }
                        break;
                    }
                    case PTG_TRIANGLE_STRIP:
                    {
                        for(int it = 0 ; it < pTP->nVert - 2 ; it++)
                        {
                            pvert_list[0].x = p_vertex[(it*2)];
                            pvert_list[0].y = p_vertex[(it*2)+1];

                            pvert_list[1].x = p_vertex[(it*2)+2];
                            pvert_list[1].y = p_vertex[(it*2)+3];

                            pvert_list[2].x = p_vertex[(it*2)+4];
                            pvert_list[2].y = p_vertex[(it*2)+5];

                            if(G_PtInPolygon((MyPoint *)pvert_list, 3, easting, northing))
                            {
                                ret = true;
                                break;
                            }
                        }
                        break;
                    }
                    case PTG_TRIANGLES:
                    {
                        for(int it = 0 ; it < pTP->nVert ; it+=3)
                        {
                            pvert_list[0].x = p_vertex[(it*2)];
                            pvert_list[0].y = p_vertex[(it*2)+1];

                            pvert_list[1].x = p_vertex[(it*2)+2];
                            pvert_list[1].y = p_vertex[(it*2)+3];

                            pvert_list[2].x = p_vertex[(it*2)+4];
                            pvert_list[2].y = p_vertex[(it*2)+5];

                            if(G_PtInPolygon((MyPoint *)pvert_list, 3, easting, northing))
                            {
                                ret = true;
                                break;
                            }
                        }
                        break;
                    }
                }

            }
            pTP = pTP->p_next;
        }

    }           // if pPolyTessGeo




    return ret;
}


//------------------------------------------------------------------------
//
//          S57 ENC (i.e. "raw") DataSet support functions
//          Not bulletproof, so call carefully
//
//------------------------------------------------------------------------
bool s57chart::InitENCMinimal(const wxString &FullPath)
{
      m_pENCDS = new OGRS57DataSource;

      if(g_poRegistrar)
            m_pENCDS->SetS57Registrar(g_poRegistrar);

      if(!m_pENCDS->OpenMin(FullPath.mb_str(), TRUE))
            return false;

      S57Reader *pENCReader = m_pENCDS->GetModule(0);
      pENCReader->SetClassBased( g_poRegistrar );

      pENCReader->Ingest();

      return true;
}


OGRFeature *s57chart::GetChartFirstM_COVR(int &catcov)
{
//    Get the reader
      S57Reader *pENCReader = m_pENCDS->GetModule(0);

      if(pENCReader)
      {

//      Select the proper class
            g_poRegistrar->SelectClass( "M_COVR");

//      Build a new feature definition for this class
            OGRFeatureDefn *poDefn = S57GenerateObjectClassDefn( g_poRegistrar, g_poRegistrar->GetOBJL(),
                                                pENCReader->GetOptionFlags() );

//      Add this feature definition to the reader
            pENCReader->AddFeatureDefn(poDefn);

//    Also, add as a Layer to Datasource to ensure proper deletion
            m_pENCDS->AddLayer( new OGRS57Layer( m_pENCDS, poDefn, 1 ) );

//      find this feature
            OGRFeature *pobjectDef = pENCReader->ReadNextFeature(poDefn );
            if(pobjectDef)
            {
      //  Fetch the CATCOV attribute
               catcov = pobjectDef->GetFieldAsInteger( "CATCOV" );
               return pobjectDef;
            }

            else
            {
               return NULL;
            }
      }
      else
            return NULL;
}

OGRFeature *s57chart::GetChartNextM_COVR(int &catcov)
{
    catcov = -1;

//    Get the reader
    S57Reader *pENCReader = m_pENCDS->GetModule(0);

//    Get the Feature Definition, stored in Layer 0
    OGRFeatureDefn *poDefn = m_pENCDS->GetLayer(0)->GetLayerDefn();

    if(pENCReader)
    {
      OGRFeature *pobjectDef = pENCReader->ReadNextFeature(poDefn);

      if(pobjectDef)
      {
        catcov = pobjectDef->GetFieldAsInteger( "CATCOV" );
        return pobjectDef;
      }

      return NULL;
    }
    else
      return NULL;

}


int s57chart::GetENCScale(void)
{
      if(NULL == m_pENCDS)
            return 0;

      //    Assume that chart has been initialized for minimal ENC access
      //    which implies that the ENC has been fully ingested, and some
      //    interesting values have been extracted thereby.

//    Get the reader
      S57Reader *pENCReader = m_pENCDS->GetModule(0);

      if(pENCReader)
            return pENCReader->GetCSCL();
      else
            return 1;
}


 extern wxLog *logger;

/************************************************************************/
/*                       OpenCPN_OGRErrorHandler()                      */
/*                       Use Global wxLog Class                         */
/************************************************************************/

void OpenCPN_OGRErrorHandler( CPLErr eErrClass, int nError,
                              const char * pszErrorMsg )
{

#define ERR_BUF_LEN 2000

    char buf[ERR_BUF_LEN + 1];

    if( eErrClass == CE_Debug )
        sprintf( buf, "%s", pszErrorMsg );
    else if( eErrClass == CE_Warning )
        sprintf( buf, "Warning %d: %s\n", nError, pszErrorMsg );
    else
        sprintf( buf, "ERROR %d: %s\n", nError, pszErrorMsg );


    wxLogMessage(_T("%s"), buf);


    //      Do not simply return on CE_Fatal errors, as we don't want to abort()

    if(eErrClass == CE_Fatal)
    {
        longjmp(env_ogrf, 1);                  // jump back to the setjmp() point
    }

}



//      In GDAL-1.2.0, CSVGetField is not exported.......
//      So, make my own simplified copy
/************************************************************************/
/*                           MyCSVGetField()                            */
/*                                                                      */
/************************************************************************/


const char *MyCSVGetField( const char * pszFilename,
                         const char * pszKeyFieldName,
                         const char * pszKeyFieldValue,
                         CSVCompareCriteria eCriteria,
                         const char * pszTargetField )

{
    char        **papszRecord;
    int         iTargetField;


/* -------------------------------------------------------------------- */
/*      Find the correct record.                                        */
/* -------------------------------------------------------------------- */
    papszRecord = CSVScanFileByName( pszFilename, pszKeyFieldName,
                                     pszKeyFieldValue, eCriteria );

    if( papszRecord == NULL )
        return "";

/* -------------------------------------------------------------------- */
/*      Figure out which field we want out of this.                     */
/* -------------------------------------------------------------------- */
    iTargetField = CSVGetFileFieldId( pszFilename, pszTargetField );
    if( iTargetField < 0 )
        return "";

    if( iTargetField >= CSLCount( papszRecord ) )
        return "";

    return( papszRecord[iTargetField] );
}



//------------------------------------------------------------------------
//  Initialize GDAL/OGR S57ENC support
//------------------------------------------------------------------------

int s57_initialize(const wxString& csv_dir, FILE *flog)
{

    //      Get one instance of the s57classregistrar,
    //      And be prepared to give it to any module that needs it

    if( g_poRegistrar == NULL )
    {
        g_poRegistrar = new S57ClassRegistrar();

        if( !g_poRegistrar->LoadInfo( csv_dir.mb_str(), FALSE ) )
        {
            delete g_poRegistrar;
            g_poRegistrar = NULL;
        }
    }

    return 0;
}


//------------------------------------------------------------------------
//
//          Some s57 Utilities
//          Meant to be called "bare", usually with no class instance.
//
//------------------------------------------------------------------------


//----------------------------------------------------------------------------------
// Get Chart Extents
//----------------------------------------------------------------------------------

bool s57_GetChartExtent(const wxString& FullPath, Extent *pext)
{
 //   Fix this  find extents of which?? layer??
/*
    OGRS57DataSource *poDS = new OGRS57DataSource;
    poDS->Open(pFullPath, TRUE);

    if( poDS == NULL )
    return false;

    OGREnvelope Env;
    S57Reader   *poReader = poDS->GetModule(0);
    poReader->GetExtent(&Env, true);

    pext->NLAT = Env.MaxY;
    pext->ELON = Env.MaxX;
    pext->SLAT = Env.MinY;
    pext->WLON = Env.MinX;

    delete poDS;
*/
    return false;

}

