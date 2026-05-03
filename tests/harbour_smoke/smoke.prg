/* OpenADS / Harbour rddads smoke test (M8.7).
 *
 * Multi-field DBF + CDX. Walk + dbSeek + APPEND BLANK + write back +
 * close + reopen + verify the appended row is visible.
 */
#include "ads.ch"

REQUEST ADS, ADSCDX, ADSNTX

PROCEDURE Main()
   ErrorBlock( {|oErr| MyHandler( oErr ) } )

   ? "OpenADS smoke test (M8.7)"
   ? "ACE DLL reports:", AdsVersion()

   AdsSetFileType( ADS_CDX )
   AdsSetServerType( ADS_LOCAL_SERVER )

   IF ! AdsConnect( "." )
      ? "AdsConnect failed."
      RETURN
   ENDIF

   USE data INDEX data VIA "ADSCDX"
   IF NetErr()
      ? "USE failed"
      RETURN
   ENDIF

   ? "Initial record count:", LastRec()

   ? "Append a fourth row..."
   dbAppend()
   FIELD->NAME   := "DELTA"
   FIELD->AGE    := 99
   FIELD->ACTIVE := .F.
   FIELD->BORN   := SToD( "20260101" )
   dbCommit()
   ? "  appended at recno", RecNo()

   ? "After append, record count:", LastRec()

   ? "Walk in NAME order:"
   dbGoTop()
   DO WHILE ! Eof()
      ? "  rec", RecNo(), ;
        "NAME=[" + FIELD->NAME + "]", ;
        "AGE=" + LTrim(Str(FIELD->AGE)), ;
        "ACTIVE=" + iif(FIELD->ACTIVE, "T", "F"), ;
        "BORN=" + DToS(FIELD->BORN)
      dbSkip()
   ENDDO

   USE

   ? ""
   ? "Reopen and re-walk to verify durability..."
   USE data INDEX data VIA "ADSCDX"
   ? "Reopened record count:", LastRec()
   dbSeek("DELTA")
   ? "  Seek 'DELTA': Found=" + iif(Found(), "T", "F"), ;
     "RecNo=" + LTrim(Str(RecNo())), ;
     "NAME=[" + FIELD->NAME + "]", ;
     "AGE=" + LTrim(Str(FIELD->AGE)), ;
     "ACTIVE=" + iif(FIELD->ACTIVE, "T", "F"), ;
     "BORN=" + DToS(FIELD->BORN)
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
