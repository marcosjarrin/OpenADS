/* OpenADS / Harbour rddads smoke test (M8.9).
 *
 * Multi-tag CDX with two tags (NAME and AGE). Smoke walks records in
 * each focus, exercises dbSeek through OrdSetFocus, and verifies the
 * Order list reports both tags.
 */
#include "ads.ch"

REQUEST ADS, ADSCDX, ADSNTX

PROCEDURE Main()
   ErrorBlock( {|oErr| MyHandler( oErr ) } )

   ? "OpenADS smoke test (M8.9)"
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
   ? "Default order :", OrdName()

   ? ""
   ? "=== Walk under NAME order ==="
   OrdSetFocus( "NAME" )
   ? "Active order:", OrdName(), "  key:", OrdKey()
   dbGoTop()
   DO WHILE ! Eof()
      ? "  rec", RecNo(), ;
        "NAME=[" + FIELD->NAME + "]", ;
        "AGE=" + LTrim(Str(FIELD->AGE))
      dbSkip()
   ENDDO

   ? ""
   ? "=== Walk under AGE order ==="
   OrdSetFocus( "AGE" )
   ? "Active order:", OrdName(), "  key:", OrdKey()
   dbGoTop()
   DO WHILE ! Eof()
      ? "  rec", RecNo(), ;
        "NAME=[" + FIELD->NAME + "]", ;
        "AGE=" + LTrim(Str(FIELD->AGE))
      dbSkip()
   ENDDO

   ? ""
   ? "=== Seek by AGE ==="
   dbSeek(" 77")
   ? "  Seek ' 77': Found=" + iif(Found(), "T", "F"), ;
     "RecNo=" + LTrim(Str(RecNo())), ;
     "NAME=[" + FIELD->NAME + "]", ;
     "AGE=" + LTrim(Str(FIELD->AGE))

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
