/* OpenADS / Harbour rddads smoke test (M8.11).
 *
 * Multi-field DBF + CDX + FPT memo. dbAppend with a memo payload,
 * then close + reopen + verify the memo round-trips intact.
 */
#include "ads.ch"

REQUEST ADS, ADSCDX, ADSNTX

PROCEDURE Main()
   LOCAL nConn

   ErrorBlock( {|oErr| MyHandler( oErr ) } )

   ? "OpenADS smoke test (M8.11)"
   ? "ACE DLL reports:", AdsVersion()

   AdsSetFileType( ADS_CDX )
   AdsSetServerType( ADS_LOCAL_SERVER )

   IF ! AdsConnect( "." )
      ? "AdsConnect failed."
      RETURN
   ENDIF
   nConn := AdsConnection()

   USE data INDEX data VIA "ADSCDX"
   IF NetErr()
      ? "USE failed"
      RETURN
   ENDIF
   ? "Initial count:", LastRec()

   /* Append a row that includes a memo payload. */
   ? "Appending row with memo..."
   dbAppend()
   FIELD->NAME   := "DELTA"
   FIELD->AGE    := 99
   FIELD->ACTIVE := .F.
   FIELD->BORN   := SToD( "20260101" )
   FIELD->NOTES  := "Hello memo from OpenADS smoke test (M8.11)."
   ? "  appended at recno", RecNo()
   ? "  in-memory NOTES len =", Len(FIELD->NOTES)

   /* Append a second row with a longer memo. */
   dbAppend()
   FIELD->NAME   := "OMEGA"
   FIELD->AGE    := 200
   FIELD->ACTIVE := .T.
   FIELD->BORN   := SToD( "20260202" )
   FIELD->NOTES  := Replicate( "Lorem ipsum dolor sit amet. ", 10 )
   ? "  appended at recno", RecNo()
   ? "  in-memory NOTES len =", Len(FIELD->NOTES)

   USE

   /* Reopen and verify the memos. */
   ? ""
   ? "=== Reopen + verify memo durability ==="
   USE data INDEX data VIA "ADSCDX"
   ? "Reopened count:", LastRec()
   OrdSetFocus( "NAME" )

   dbSeek( "DELTA" )
   ? "  Seek 'DELTA': Found=" + iif(Found(), "T", "F"), ;
     "len=" + LTrim(Str(Len(FIELD->NOTES)))
   ? "    NOTES = [" + FIELD->NOTES + "]"

   dbSeek( "OMEGA" )
   ? "  Seek 'OMEGA': Found=" + iif(Found(), "T", "F"), ;
     "len=" + LTrim(Str(Len(FIELD->NOTES)))
   ? "    NOTES first 60 = [" + Left(FIELD->NOTES, 60) + "]"

   USE
   AdsDisconnect( nConn )
   ? "Done."
   RETURN

PROCEDURE MyHandler( oErr )
   ? "ERROR:", oErr:Description, "[" + LTrim(Str(oErr:GenCode)) + "/" + ;
        LTrim(Str(oErr:SubCode)) + "]"
   ? "Operation:", oErr:Operation
   ErrorLevel( 1 )
   QUIT
   RETURN
