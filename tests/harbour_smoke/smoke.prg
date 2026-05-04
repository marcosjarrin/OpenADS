/* OpenADS / Harbour rddads smoke test (M9.3).
 *
 * Compound CDX index expressions: a UPPER(NAME) tag stays in sync
 * across dbAppend with a record whose NAME is mixed case. After
 * close + reopen the smoke seeks the new key via the upper-case
 * tag and confirms the record is reachable.
 */
#include "ads.ch"

REQUEST ADS, ADSCDX, ADSNTX

PROCEDURE Main()
   ErrorBlock( {|oErr| MyHandler( oErr ) } )

   ? "OpenADS smoke test (M9.3)"
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

   ? "Number of orders:", OrdCount()
   ? ""
   ? "=== Walk via UPPER_NAME (compound key) ==="
   OrdSetFocus( "UPPER_NAME" )
   ? "Active order:", OrdName(), "  key:", OrdKey()
   dbGoTop()
   DO WHILE ! Eof()
      ? "  rec", RecNo(), ;
        "NAME=[" + FIELD->NAME + "]", ;
        "AGE=" + LTrim(Str(FIELD->AGE))
      dbSkip()
   ENDDO

   ? ""
   ? "=== Append a mixed-case row, expect UPPER index sync ==="
   dbAppend()
   FIELD->NAME   := "delta"
   FIELD->AGE    := 99
   FIELD->ACTIVE := .F.
   FIELD->BORN   := SToD( "20260101" )
   FIELD->NOTES  := ""
   ? "  appended at recno", RecNo()

   USE

   ? ""
   ? "=== Reopen + seek via UPPER_NAME ==="
   USE data INDEX data VIA "ADSCDX"
   OrdSetFocus( "UPPER_NAME" )

   dbSeek( "DELTA" )
   ? "  Seek 'DELTA' (upper) : Found=" + iif(Found(), "T", "F"), ;
     iif(Found(), "RecNo=" + LTrim(Str(RecNo())) + ;
                  " NAME=[" + FIELD->NAME + "]", "")

   dbSeek( "delta" )
   ? "  Seek 'delta' (lower) : Found=" + iif(Found(), "T", "F"), ;
     "(should be F)"

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
