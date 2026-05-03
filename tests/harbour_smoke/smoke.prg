/* OpenADS / Harbour rddads smoke test (M8.5).
 *
 * Multi-field DBF: NAME C(10), AGE N(3,0), ACTIVE L(1), BORN D(8).
 * Walks every record and prints all four field values, exercising the
 * Character / Numeric / Logical / Date code paths in OpenADS through
 * Harbour's contrib/rddads.
 */
#include "ads.ch"

REQUEST ADS, ADSCDX, ADSNTX

PROCEDURE Main()
   LOCAL nFld

   ErrorBlock( {|oErr| MyHandler( oErr ) } )

   ? "OpenADS smoke test (M8.5)"
   ? "ACE DLL reports:", AdsVersion()

   AdsSetFileType( ADS_CDX )
   AdsSetServerType( ADS_LOCAL_SERVER )

   IF ! AdsConnect( "." )
      ? "AdsConnect failed."
      RETURN
   ENDIF

   USE data VIA "ADSCDX"
   IF NetErr()
      ? "USE failed"
      RETURN
   ENDIF

   ? "Schema:"
   FOR nFld := 1 TO FCount()
      ? "  ", nFld, FieldName(nFld), ;
        FieldType(nFld), "len=", FieldLen(nFld), "dec=", FieldDec(nFld)
   NEXT

   ? "Walking", LastRec(), "records:"
   DO WHILE ! Eof()
      ? "  rec", RecNo(), ;
        "NAME=[" + FIELD->NAME + "]", ;
        "AGE=" + LTrim(Str(FIELD->AGE)), ;
        "ACTIVE=" + iif(FIELD->ACTIVE, "T", "F"), ;
        "BORN=" + DToS(FIELD->BORN)
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
