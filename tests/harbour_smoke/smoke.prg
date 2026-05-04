/* OpenADS / Harbour rddads smoke test (M8.10).
 *
 * Transaction surface: BEGIN + APPEND + ROLLBACK should leave the
 * table unchanged; BEGIN + APPEND + COMMIT should persist the new
 * row across close/reopen.
 */
#include "ads.ch"

REQUEST ADS, ADSCDX, ADSNTX

PROCEDURE Main()
   LOCAL nConn

   ErrorBlock( {|oErr| MyHandler( oErr ) } )

   ? "OpenADS smoke test (M8.10)"
   ? "ACE DLL reports:", AdsVersion()

   AdsSetFileType( ADS_CDX )
   AdsSetServerType( ADS_LOCAL_SERVER )

   IF ! AdsConnect( "." )
      ? "AdsConnect failed."
      RETURN
   ENDIF
   nConn := AdsConnection()
   ? "Connection handle:", nConn

   USE data INDEX data VIA "ADSCDX"
   IF NetErr()
      ? "USE failed"
      RETURN
   ENDIF
   ? "Initial count:", LastRec()

   /* === BEGIN + APPEND + ROLLBACK === */
   ? ""
   ? "=== Tx 1: append EPSILON, then rollback ==="
   AdsBeginTransaction( nConn )
   dbAppend()
   FIELD->NAME   := "EPSILON"
   FIELD->AGE    := 50
   FIELD->ACTIVE := .T.
   FIELD->BORN   := SToD( "20300101" )
   ? "  inside tx, count =", LastRec()
   AdsRollback( nConn )
   ? "  after rollback, count =", LastRec()

   /* === BEGIN + APPEND + COMMIT === */
   ? ""
   ? "=== Tx 2: append ZETA, then commit ==="
   AdsBeginTransaction( nConn )
   dbAppend()
   FIELD->NAME   := "ZETA"
   FIELD->AGE    := 60
   FIELD->ACTIVE := .F.
   FIELD->BORN   := SToD( "20300202" )
   ? "  inside tx, count =", LastRec()
   AdsCommitTransaction( nConn )
   ? "  after commit, count =", LastRec()

   USE

   /* === Reopen + verify durability === */
   ? ""
   ? "=== Reopen + verify ==="
   USE data INDEX data VIA "ADSCDX"
   ? "Reopened count:", LastRec()

   /* Default order is AGE (alphabetically first sub-tag); switch to
    * NAME so dbSeek looks up by character key. */
   OrdSetFocus( "NAME" )
   SET DELETED ON

   dbSeek( "ZETA" )
   ? "  Seek 'ZETA'   : Found=" + iif(Found(), "T", "F") + ;
     "  RecNo=" + LTrim(Str(RecNo())) + ;
     "  AGE="    + LTrim(Str(FIELD->AGE)) + ;
     "  Deleted=" + iif(Deleted(),"T","F")
   dbSeek( "EPSILON" )
   ? "  Seek 'EPSILON': Found=" + iif(Found(), "T", "F") + ;
     "  RecNo=" + LTrim(Str(RecNo())) + ;
     "  AGE="    + LTrim(Str(FIELD->AGE)) + ;
     "  Deleted=" + iif(Deleted(),"T","F")

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
