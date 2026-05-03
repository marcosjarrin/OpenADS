/* OpenADS / Harbour rddads smoke test (M8.3).
 *
 * Goal: prove a real Harbour app — using the standard Clipper RDD
 * surface — can open a DBF table through OpenADS and walk it. The
 * fixture data.dbf is pre-staged on disk by run_build.bat (a tiny
 * binary blob; no Harbour-side creation needed, so this exercises the
 * read path end-to-end without relying on AdsCreateTable / DBFCDX).
 */
#include "ads.ch"

REQUEST ADS, ADSCDX, ADSNTX

PROCEDURE Main()
   LOCAL nRec
   LOCAL cName
   LOCAL nConn

   ErrorBlock( {|oErr| MyHandler( oErr ) } )

   ? "OpenADS smoke test (M8.3)"
   ? "ACE DLL reports:", AdsVersion()

   AdsSetFileType( ADS_CDX )
   AdsSetServerType( ADS_LOCAL_SERVER )

   /* AdsConnect (Harbour-side wrapper) calls AdsConnect60 and stashes
    * the handle as the default rddads connection. Without it, USE ...
    * VIA "ADSCDX" fails with rddads error 4097 "unknown connection". */
   nConn := AdsConnect( "." )
   ? "AdsConnect handle:", nConn

   ? "Opening data.dbf VIA ADSCDX..."
   USE data VIA "ADSCDX"
   IF NetErr()
      ? "USE failed"
      RETURN
   ENDIF

   ? "Field count:", FCount()
   ? "Field 1 name :", FieldName(1)
   ? "Field 1 type :", FieldType(1), "len=", FieldLen(1)
   ? "Record count:", LastRec()

   ? "Walking records:"
   DO WHILE ! Eof()
      nRec  := RecNo()
      cName := FIELD->NAME
      ? "  rec", nRec, "name=[", cName, "]"
      dbSkip()
   ENDDO

   USE

   ? "Done."
   RETURN

PROCEDURE MyHandler( oErr )
   ? "ERROR:", oErr:Description, "[" + LTrim(Str(oErr:GenCode)) + "/" + ;
        LTrim(Str(oErr:SubCode)) + "]"
   ? "Operation:", oErr:Operation
   ErrorLevel( 1 )
   QUIT
   RETURN
