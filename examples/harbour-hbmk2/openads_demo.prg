/*
 * openads_demo.prg — minimal console app proving the standard Harbour
 * rddads stack reaches OpenADS' ACE DLL instead of a SAP-shipped one.
 *
 * Build:    hbmk2 openads_demo.hbp     (after `set OPENADS_LIB=...`)
 * Run:      openads_demo.exe
 *
 * Make sure OpenADS' ace64.dll (or ace32.dll) is the first one
 * resolved on PATH / next to the produced .exe.
 */
#include "ads.ch"
#include "rddsys.ch"

REQUEST ADS, ADSCDX, ADSNTX

PROCEDURE Main()

   LOCAL cDir := hb_DirTemp() + "openads_demo"
   LOCAL cDbf := cDir + hb_ps() + "cust.dbf"

   ? "OpenADS hbmk2 demo"
   ? "ACE DLL reports:", AdsVersion()
   ?

   AdsSetServerType( ADS_LOCAL_SERVER )
   AdsSetFileType( ADS_CDX )
   RddSetDefault( "ADSCDX" )

   hb_DirCreate( cDir )

   IF ! AdsConnect( cDir )
      ? "AdsConnect failed:", DosError()
      ErrorLevel( 1 )
      QUIT
   ENDIF

   IF File( cDbf )
      FErase( cDbf )
      FErase( hb_FNameExtSet( cDbf, ".cdx" ) )
   ENDIF

   DbCreate( cDbf, { ;
       { "ID",   "N",  6, 0 }, ;
       { "NAME", "C", 30, 0 }, ;
       { "AGE",  "N",  3, 0 } }, "ADSCDX" )

   USE ( cDbf ) VIA "ADSCDX" NEW EXCLUSIVE
   INDEX ON UPPER( FIELD->NAME ) TAG UPPER_NAME

   AAdd( aData := {}, { 1, "alice", 30 } )
   AAdd( aData,       { 2, "BOB",   25 } )
   AAdd( aData,       { 3, "delta", 99 } )

   AEval( aData, {| row | ;
       DbAppend(), ;
       FIELD->ID   := row[ 1 ], ;
       FIELD->NAME := row[ 2 ], ;
       FIELD->AGE  := row[ 3 ] } )
   DbCommit()

   ? "Rows after append:", LastRec()
   ?
   ? "Walk via UPPER_NAME (compound CDX):"
   OrdSetFocus( "UPPER_NAME" )
   dbGoTop()
   DO WHILE ! Eof()
      ? "  rec", RecNo(), "name=[" + FIELD->NAME + "] age=" + ;
        LTrim( Str( FIELD->AGE ) )
      dbSkip()
   ENDDO
   ?
   dbSeek( "DELTA" )
   ? "Seek 'DELTA' (upper):", iif( Found(), "Found rec " + LTrim( Str( RecNo() ) ), "not found" )

   DbCloseArea()
   AdsDisconnect()

   ? "Done."
   RETURN
