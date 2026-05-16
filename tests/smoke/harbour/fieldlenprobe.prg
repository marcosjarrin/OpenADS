// fieldlenprobe.prg — reproduce the mini_xbrowse "/ads" column-width
// truncation report (Pritpal). Stages a DBF with wide CHAR fields,
// then dumps FCount / dbStruct / FieldLen and the row values both
// via the DBFCDX baseline and via ADSCDX -> OpenADS.
//
// Build: see fieldlenprobe_build.bat
// Run:   fieldlenprobe.exe          (DBFCDX baseline)
//        fieldlenprobe.exe /ads     (ADSCDX -> OpenADS)

#require "rddads"

REQUEST DBFCDX
REQUEST ADSCDX

PROCEDURE Main( cMode )

   LOCAL cDir := TempFolder() + "\openads_fieldlen"
   LOCAL cDbf := cDir + "\people.dbf"
   LOCAL lAds := ( ValType( cMode ) == "C" .AND. "/ads" $ Lower( cMode ) )
   LOCAL aStru, i, xVal

   StageDbf( cDir, cDbf )

   IF lAds
      AdsSetFileType( 2 )        // ADS_CDX / FoxPro
      AdsSetServerType( 1 )      // ADS_LOCAL_SERVER
      IF ! AdsConnect( cDir )
         ? "AdsConnect failed for " + cDir
         RETURN
      ENDIF
      RddSetDefault( "ADSCDX" )
      USE ( "people" ) VIA "ADSCDX" ALIAS T SHARED NEW
   ELSE
      RddSetDefault( "DBFCDX" )
      USE ( cDbf ) ALIAS T SHARED NEW
   ENDIF
   IF Select( "T" ) == 0
      ? "USE failed"
      RETURN
   ENDIF

   ? "=== field-length probe — RDD=" + T->( RddName() ) + " ==="
   ? "FCount=" + Str( T->( FCount() ), 3 )
   ?
   ? "dbStruct() / FieldLen():"
   aStru := T->( DbStruct() )
   FOR i := 1 TO Len( aStru )
      ? "  " + PadR( aStru[ i ][ 1 ], 8 ) + ;
        " type=" + aStru[ i ][ 2 ] + ;
        " struct.len=" + Str( aStru[ i ][ 3 ], 3 ) + ;
        " dec=" + Str( aStru[ i ][ 4 ], 2 ) + ;
        "   FieldLen(" + Str( i, 1 ) + ")=" + Str( T->( FieldLen( i ) ), 3 )
   NEXT
   ?
   ? "rows — value + Len( FieldGet ):"
   T->( DbGoTop() )
   DO WHILE ! T->( Eof() )
      xVal := T->NAME
      ?? " rec" + Str( T->( RecNo() ), 2 ) + ;
         "  NAME=[" + xVal + "] len=" + Str( Len( xVal ), 2 )
      xVal := T->CITY
      ?? "  CITY=[" + xVal + "] len=" + Str( Len( xVal ), 2 )
      ?
      T->( DbSkip() )
   ENDDO

   CLOSE ALL
   RETURN

//----------------------------------------------------------------------

STATIC FUNCTION TempFolder()
   LOCAL c := GetEnv( "TEMP" )
   IF Empty( c ) ; c := GetEnv( "TMP" ) ; ENDIF
   IF Empty( c ) ; c := "C:\Temp" ; ENDIF
   RETURN c

STATIC PROCEDURE StageDbf( cDir, cDbf )
   LOCAL aRows := { { "Alice",   "Madrid"    }, ;
                    { "Charlie", "Barcelona" }, ;
                    { "Edward",  "Valencia"  }, ;
                    { "Bob",     "Sevilla"   }, ;
                    { "Diana",   "Bilbao"    } }
   LOCAL aRow
   IF ! hb_DirExists( cDir ) ; hb_DirCreate( cDir ) ; ENDIF
   IF File( cDbf ) ; FErase( cDbf ) ; ENDIF
   IF File( hb_FNameExtSet( cDbf, ".cdx" ) )
      FErase( hb_FNameExtSet( cDbf, ".cdx" ) )
   ENDIF
   // NAME C(20), CITY C(25) — deliberately wide so a wrong
   // reported length is obvious.
   DbCreate( cDbf, { { "NAME", "C", 20, 0 }, ;
                     { "CITY", "C", 25, 0 } } )
   USE ( cDbf ) ALIAS _S SHARED NEW
   FOR EACH aRow IN aRows
      _S->( DbAppend() )
      _S->NAME := aRow[ 1 ]
      _S->CITY := aRow[ 2 ]
   NEXT
   _S->( DbCommit() )
   _S->( DbCloseArea() )
   RETURN
