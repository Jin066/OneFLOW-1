/*---------------------------------------------------------------------------*\
    OneFLOW - LargeScale Multiphysics Scientific Simulation Environment
    Copyright (C) 2017-2020 He Xin and the OneFLOW contributors.
-------------------------------------------------------------------------------
License
    This file is part of OneFLOW.

    OneFLOW is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OneFLOW is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OneFLOW.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "UITimeStep.h"
#include "INsCom.h"
#include "UINsCom.h"
#include "UCom.h"
#include "TurbCom.h"
#include "UTurbCom.h"
#include "INsCtrl.h"
#include "Zone.h"
#include "Grid.h"
#include "UnsGrid.h"
#include "FaceTopo.h"
#include "FaceMesh.h"
#include "CellMesh.h"
#include "HXMath.h"
#include "DataBase.h"
#include "FieldBase.h"
#include "Iteration.h"
#include <iostream>
using namespace std;

BeginNameSpace( ONEFLOW )





UITimestep::UITimestep()
{
    ;
}

UITimestep::~UITimestep()
{
    ;
}

void UITimestep::Init()
{
    ug.Init();
    uinsf.Init();
}

void UITimestep::ReadTmp()
{
    static int iii = 0;
    if ( iii ) return;
    iii = 1;
    fstream file;
    file.open( "nsflow.dat", ios_base::in | ios_base::binary );
    if ( ! file ) exit( 0 );

    uinsf.Init();

       for ( int cId = 0; cId < ug.nTCell; ++ cId )
    {
        for ( int iEqu = 0; iEqu < 5; ++ iEqu )
        {
            file.read( reinterpret_cast< char * >( & ( * uinsf.q )[ iEqu ][ cId ] ), sizeof( double ) );
        }
    }

       for ( int cId = 0; cId < ug.nTCell; ++ cId )
    {
        file.read( reinterpret_cast< char * >( & ( * uinsf.visl )[ 0 ][ cId ] ), sizeof( double ) );
    }

       for ( int cId = 0; cId < ug.nTCell; ++ cId )
    {
        file.read( reinterpret_cast< char * >( & ( * uinsf.vist )[ 0 ][ cId ] ), sizeof( double ) );
    }

    vector< Real > tmp1( ug.nTCell ), tmp2( ug.nTCell );

       for ( int cId = 0; cId < ug.nTCell; ++ cId )
    {
        tmp1[ cId ] = ( * uinsf.timestep )[ 0 ][ cId ];
    }

       for ( int cId = 0; cId < ug.nTCell; ++ cId )
    {
        file.read( reinterpret_cast< char * >( & ( * uinsf.timestep )[ 0 ][ cId ] ), sizeof( double ) );
    }

       for ( int cId = 0; cId < ug.nTCell; ++ cId )
    {
        tmp2[ cId ] = ( * uinsf.timestep )[ 0 ][ cId ];
    }

    for ( int iCell = 0; iCell < ug.nTCell; ++ iCell )
    {
        for ( int iEqu = 0; iEqu < nscom.nTModel; ++ iEqu )
        {
            file.read( reinterpret_cast< char * >( & ( * uinsf.tempr )[ iEqu ][ iCell ] ), sizeof( double ) );
        }
    }

    turbcom.Init();
    uturbf.Init();
    for ( int iCell = 0; iCell < ug.nTCell; ++ iCell )
    {
        for ( int iEqu = 0; iEqu < turbcom.nEqu; ++ iEqu )
        {
            file.read( reinterpret_cast< char * >( & ( * uturbf.q )[ iEqu ][ iCell ] ), sizeof( double ) );
        }
    }
    file.close();
    file.clear();
}


void UITimestep::CmpTimestep()
{
    this->Init();

    //ReadTmp();

    this->CmpCfl();

    this->CmpSpectrumField();

    if ( nscom.timestepModel == 0 )
    {
        this->CmpLocalTimestep();
    }
    else if ( nscom.timestepModel == 1 )
    {
        this->CmpGlobalTimestep();
    }
    else
    {
        this->CmpLgTimestep();
    }
}

void UITimestep::CmpLocalTimestep()
{
    this->CmpInvTimestep();

    this->CmpVisTimestep();

    this->ModifyTimestep();
}

void UITimestep::CmpInvTimestep()
{
    for ( int cId = 0; cId < ug.nCell; ++ cId )
    {
        ug.cId  = cId;
        gcom.cvol  = ( * ug.cvol )[ cId ];

        nscom.invsr = ( * uinsf.invsr )[ 0 ][ cId ];
        this->CmpCellInvTimestep();
        ( * uinsf.timestep )[ 0 ][ cId ] = nscom.timestep;
    }
}

void UITimestep::CmpVisTimestep()
{
    bool flag = vis_model.vismodel > 0 && nscom.visTimeStepModel > 0;

    if ( ! flag ) return;

    for ( int cId = 0; cId < ug.nCell; ++ cId )
    {
        ug.cId  = cId;
        gcom.cvol  = ( * ug.cvol )[ cId ];
        nscom.vissr = ( * uinsf.vissr )[ 0 ][ cId ];
        nscom.timestep = ( * uinsf.timestep )[ 0 ][ cId ];
        this->CmpCellVisTimestep();
        ( * uinsf.timestep )[ 0 ][ cId ] = nscom.timestep;
    }
}

void UITimestep::CmpSpectrumField()
{
    this->CmpInvSpectrumField();
    this->CmpVisSpectrumField();
}

void UITimestep::CmpInvSpectrumField()
{
    Grid * grid = Zone::GetGrid();

    MRField * invsr = ONEFLOW::GetFieldPointer< MRField >( grid, "invsr" );

    ONEFLOW::ZeroField( invsr, 1, grid->nCell );

    for ( int iFace = 0; iFace < grid->nFace; ++ iFace )
    {
        this->SetId( iFace );

        this->PrepareData();

        this->CmpFaceInvSpec();

        this->UpdateInvSpectrumField();
    }
}

void UITimestep::CmpVisSpectrumField()
{
    Grid * grid = Zone::GetGrid();

    MRField * vissr = ONEFLOW::GetFieldPointer< MRField >( grid, "vissr" );

    ONEFLOW::ZeroField( vissr, 1, grid->nCell );

    if ( vis_model.vismodel <= 0 ) return;

    for ( int iFace = 0; iFace < grid->nFace; ++ iFace )
    {
        this->SetId( iFace );

        this->PrepareVisData();

        this->CmpFaceVisSpec();

        this->UpdateVisSpectrumField();
    }
}

void UITimestep::SetId( int fId )
{
    ug.fId = fId;
    ug.lc = ( * ug.lcf )[ fId ];
    ug.rc = ( * ug.rcf )[ fId ];
}

void UITimestep::PrepareData()
{
    gcom.xfn   = ( * ug.xfn   )[ ug.fId ];
    gcom.yfn   = ( * ug.yfn   )[ ug.fId ];
    gcom.zfn   = ( * ug.zfn   )[ ug.fId ];
    gcom.vfn   = ( * ug.vfn   )[ ug.fId ];
    gcom.farea = ( * ug.farea )[ ug.fId ];

    for ( int iEqu = 0; iEqu < nscom.nTEqu; ++ iEqu )
    {
        nscom.q1[ iEqu ] = ( * uinsf.q )[ iEqu ][ ug.lc ];
        nscom.q2[ iEqu ] = ( * uinsf.q )[ iEqu ][ ug.rc ];
    }

    nscom.gama1 = ( * uinsf.gama  )[ 0 ][ ug.lc ];
    nscom.gama2 = ( * uinsf.gama  )[ 0 ][ ug.rc ];
}

void UITimestep::PrepareVisData()
{
    gcom.xfn   = ( * ug.xfn   )[ ug.fId ];
    gcom.yfn   = ( * ug.yfn   )[ ug.fId ];
    gcom.zfn   = ( * ug.zfn   )[ ug.fId ];
    gcom.vfn   = ( * ug.vfn   )[ ug.fId ];
    gcom.farea = ( * ug.farea )[ ug.fId ];

    for ( int iEqu = 0; iEqu < nscom.nTEqu; ++ iEqu )
    {
        nscom.q1[ iEqu ] = ( * uinsf.q )[ iEqu ][ ug.lc ];
        nscom.q2[ iEqu ] = ( * uinsf.q )[ iEqu ][ ug.rc ];
    }

    nscom.gama1 = ( * uinsf.gama )[ 0 ][ ug.lc ];
    nscom.gama2 = ( * uinsf.gama )[ 0 ][ ug.rc ];

    gcom.cvol1 = ( * ug.cvol )[ ug.lc ];
    gcom.cvol2 = ( * ug.cvol )[ ug.rc ];

    nscom.visl1   = ( * uinsf.visl  )[ 0 ][ ug.lc ];
    nscom.visl2   = ( * uinsf.visl  )[ 0 ][ ug.rc ];

    nscom.visl = half * ( nscom.visl1 + nscom.visl2 );

    nscom.vist1 = ( * uinsf.vist )[ 0 ][ ug.lc ];
    nscom.vist2 = ( * uinsf.vist )[ 0 ][ ug.rc ];

    nscom.vist = half * ( nscom.vist1 + nscom.vist2 );

    gcom.xcc1 = ( * ug.xcc )[ ug.lc ];
    gcom.xcc2 = ( * ug.xcc )[ ug.rc ];

    gcom.ycc1 = ( * ug.ycc )[ ug.lc ];
    gcom.ycc2 = ( * ug.ycc )[ ug.rc ];

    gcom.zcc1 = ( * ug.zcc )[ ug.lc ];
    gcom.zcc2 = ( * ug.zcc )[ ug.rc ];
}

void UITimestep::UpdateInvSpectrumField()
{
    ( * uinsf.invsr )[ 0 ][ ug.lc ] += nscom.invsr;
    ( * uinsf.invsr )[ 0 ][ ug.rc ] += nscom.invsr;
}

void UITimestep::UpdateVisSpectrumField()
{
    ( * uinsf.vissr )[ 0 ][ ug.lc ] += nscom.vissr;
    ( * uinsf.vissr )[ 0 ][ ug.rc ] += nscom.vissr;
}

void UITimestep::ModifyTimestep()
{
    this->CmpMinTimestep();
    if ( nscom.max_time_ratio <= 0 ) return;

    Real maxPermittedTimestep = nscom.max_time_ratio * nscom.minTimeStep;
    for ( int cId = 0; cId < ug.nCell; ++ cId )
    {
        ( * uinsf.timestep )[ 0 ][ cId ] = MIN( ( * uinsf.timestep )[ 0 ][ cId ], maxPermittedTimestep );
    }
}

void UITimestep::CmpGlobalTimestep()
{
    this->SetTimestep( ctrl.pdt );
}

void UITimestep::CmpMinTimestep()
{
    nscom.minTimeStep = LARGE;
    for ( int cId = 0; cId < ug.nCell; ++ cId )
    {
        nscom.minTimeStep = MIN( ( * uinsf.timestep )[ 0 ][ cId ], nscom.minTimeStep );
    }
}

void UITimestep::SetTimestep( Real timestep )
{
    for ( int cId = 0; cId < ug.nCell; ++ cId )
    {
        ( * uinsf.timestep )[ 0 ][ cId ] = timestep;
    }
}

void UITimestep::CmpLgTimestep()
{
    this->CmpLocalTimestep();
    this->SetTimestep( nscom.minTimeStep );
    ctrl.pdt = nscom.minTimeStep;
}


EndNameSpace