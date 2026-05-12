// xbrowse_ads.prg — FiveWin + xBrowse over an Advantage table, driven
// by Harbour's rddads contrib linked against OpenADS' ace64.dll
// (instead of a SAP-shipped ACE).
//
// It stages a small .dbf in a temp dir, opens it through the ADSCDX
// RDD, builds an indexed order, and shows it in an xBrowse. With
// `/auto` on the command line the window closes itself after walking
// the browse (GoBottom/GoTop/Seek) — handy for a quick "does it run
// against OpenADS' DLL" check; without it, it's a normal interactive
// window.
//
// Build: see build64.cmd in this directory (FWH bcc64 toolchain +
// Harbour rddads + OpenADS' ace64). Put OpenADS' ace64.dll on PATH
// (or next to the produced .exe) — NOT a SAP one.

#include "FiveWin.ch"
#include "xbrowse.ch"

REQUEST ADSCDX
REQUEST DBFCDX

#define ADS_LOCAL_SERVER 1
#define ADS_CDX          2

//----------------------------------------------------------------------------//

FUNCTION Main( cMode )

   LOCAL oWnd, oBrw
   LOCAL lAuto := ( ValType( cMode ) == "C" .AND. Lower( cMode ) == "/auto" )
   LOCAL cDir  := TempFolder() + "\openads_fwh_demo"
   LOCAL cDbf  := cDir + "\customer.dbf"
   LOCAL cCdx  := cDir + "\customer.cdx"

   StageDbf( cDir, cDbf )

   AdsSetServerType( ADS_LOCAL_SERVER )
   AdsSetFileType( ADS_CDX )
   RddSetDefault( "ADSCDX" )

   USE ( cDbf ) VIA "ADSCDX" ALIAS CUST SHARED NEW
   IF Select( "CUST" ) == 0
      ? "FAIL: USE customer VIA ADSCDX"
      RETURN 1
   ENDIF

   IF !File( cCdx )
      INDEX ON CUST->NAME TAG NAME TO ( cCdx )
      INDEX ON CUST->CITY TAG CITY TO ( cCdx )
   ELSE
      SET INDEX TO ( cCdx )
   ENDIF
   OrdSetFocus( "NAME" )
   CUST->( DbGoTop() )

   DEFINE WINDOW oWnd TITLE "OpenADS — FWH xBrowse over ADS (" + ;
      AdsVersion() + ")" FROM 2, 2 TO 28, 96

   @ 0, 0 XBROWSE oBrw OF oWnd ALIAS "CUST" ;
      COLUMNS "NAME", "CITY", "AGE" ;
      AUTOSORT CELL LINES NOBORDER

   oBrw:CreateFromCode()
   oWnd:oClient := oBrw

   ACTIVATE WINDOW oWnd ;
      ON INIT ( oBrw:SetFocus(), ;
                iif( lAuto, AutoWalk( oBrw, oWnd ), nil ) )

   CUST->( DbCloseArea() )
   ? "OK: FWH xBrowse + ADS (via OpenADS) ran"
   RETURN 0

//----------------------------------------------------------------------------//
// In /auto mode: walk the browse through the RDD, then close the window.

STATIC FUNCTION AutoWalk( oBrw, oWnd )

   oBrw:GoBottom()
   oBrw:Refresh()
   SysRefresh()
   oBrw:GoTop()
   oBrw:Refresh()
   SysRefresh()
   CUST->( OrdSetFocus( "CITY" ) )
   CUST->( DbGoTop() )
   oBrw:Refresh()
   SysRefresh()
   oWnd:End()
   RETURN nil

//----------------------------------------------------------------------------//

STATIC FUNCTION TempFolder()
   LOCAL c := GetEnv( "TEMP" )
   IF Empty( c ) ; c := GetEnv( "TMP" ) ; ENDIF
   IF Empty( c ) ; c := "C:\Temp" ; ENDIF
   RETURN c

//----------------------------------------------------------------------------//
// Stage a 3-field .dbf with a handful of rows (only if it doesn't exist).

STATIC FUNCTION StageDbf( cDir, cDbf )

   LOCAL aStru, aData, aRec

   IF !lIsDir( cDir ) ; MakeDir( cDir ) ; ENDIF
   IF File( cDbf ) ; RETURN nil ; ENDIF

   aStru := { { "NAME", "C", 20, 0 }, ;
              { "CITY", "C", 20, 0 }, ;
              { "AGE" , "N",  3, 0 } }
   DbCreate( cDbf, aStru, "ADSCDX" )

   aData := { { "Alice"  , "Madrid"   , 30 }, ;
              { "Bob"    , "Barcelona", 41 }, ;
              { "Charlie", "Valencia" , 25 }, ;
              { "Diana"  , "Sevilla"  , 37 }, ;
              { "Edward" , "Bilbao"   , 52 } }

   USE ( cDbf ) VIA "ADSCDX" ALIAS _STG SHARED NEW
   FOR EACH aRec IN aData
      _STG->( DbAppend() )
      _STG->NAME := aRec[ 1 ]
      _STG->CITY := aRec[ 2 ]
      _STG->AGE  := aRec[ 3 ]
   NEXT
   _STG->( DbCommit() )
   _STG->( DbCloseArea() )
   RETURN nil

//----------------------------------------------------------------------------//
