// posprobe.prg — instrumented probe for the rddads ADSCDX positioning
// desync behind "ADSCDX/5000 table not positioned on a record".
//
// After each navigation op it prints the rddads view (Eof/Bof/RecNo)
// and then attempts a FieldGet inside an error trap. A FieldGet that
// raises ADSCDX/5000 while Eof()==.F. AND Bof()==.F. is the DESYNC:
// rddads believes the area is positioned, the OpenADS engine does not.
//
// Build: hbmk2 posprobe.prg -comp=msvc64 -gtstd
// Run:   posprobe.exe          (no index)
//        posprobe.exe /idx     (controlling NAME index)

#require "rddads"

REQUEST ADSCDX
REQUEST ADSKEYNO, ADSKEYCOUNT, ADSGETRELKEYPOS, ADSSETRELKEYPOS

STATIC s_nDesync := 0

PROCEDURE Main( cMode )

   LOCAL cDir := TempFolder() + "\openads_posprobe"
   LOCAL cDbf := cDir + "\probe.dbf"
   LOCAL lIdx := ( ValType( cMode ) == "C" .AND. "/idx" $ Lower( cMode ) )
   LOCAL i, nRec

   StageDbf( cDir, cDbf )

   AdsSetServerType( 1 )       // ADS_LOCAL_SERVER
   AdsSetFileType( 2 )         // ADS_CDX
   RddSetDefault( "ADSCDX" )

   USE ( cDbf ) VIA "ADSCDX" ALIAS T SHARED NEW
   IF Select( "T" ) == 0
      ? "FAIL: USE VIA ADSCDX"
      RETURN
   ENDIF

   IF lIdx
      INDEX ON T->NAME TAG NAME
      OrdSetFocus( "NAME" )
   ENDIF

   nRec := T->( RecCount() )
   ? "=== rddads/OpenADS positioning probe ==="
   ? "RDD=" + T->( RddName() ) + "  RecCount=" + Str( nRec, 3 ) + ;
     "  indexed=" + iif( lIdx, "YES (NAME)", "no" )
   ? "fmt: <op>  Eof Bof RecNo  -> FieldGet result"
   ?

   // --- forward walk off the end -------------------------------------
   Probe( "GoTop",        {|| T->( DbGoTop() ) } )
   FOR i := 1 TO nRec + 3
      Probe( "Skip(+1) #" + LTrim( Str( i ) ), {|| T->( DbSkip( 1 ) ) } )
   NEXT

   // --- recover from EOF ---------------------------------------------
   Probe( "Skip(-1)",     {|| T->( DbSkip( -1 ) ) } )
   Probe( "Skip(0)",      {|| T->( DbSkip( 0 ) ) } )

   // --- bottom / top edges -------------------------------------------
   Probe( "GoBottom",     {|| T->( DbGoBottom() ) } )
   Probe( "Skip(+1)@bot", {|| T->( DbSkip( 1 ) ) } )
   Probe( "Skip(-1)back", {|| T->( DbSkip( -1 ) ) } )
   Probe( "GoTop2",       {|| T->( DbGoTop() ) } )
   Probe( "Skip(-1)@top", {|| T->( DbSkip( -1 ) ) } )

   // --- large jumps (TBrowse measuring the window) -------------------
   Probe( "GoTop3",       {|| T->( DbGoTop() ) } )
   Probe( "Skip(+999)",   {|| T->( DbSkip( 999 ) ) } )
   Probe( "Skip(-999)",   {|| T->( DbSkip( -999 ) ) } )

   // --- seek probes (only meaningful with the index) -----------------
   IF lIdx
      Probe( "Seek hit",   {|| T->( DbSeek( "Name00100" ) ) } )
      Probe( "Seek miss",  {|| T->( DbSeek( "ZZZZ" ) ) } )
      Probe( "Seek soft",  {|| T->( DbSeek( "ZZZZ", .T. ) ) } )
      Probe( "Seek empty", {|| T->( DbSeek( "" ) ) } )
      Probe( "after seek Skip(-1)", {|| T->( DbSkip( -1 ) ) } )
   ENDIF

   // --- RelKeyPos: the path FWH/TBrowse uses for >200-row tables -----
   Probe( "GoTopRK",      {|| T->( DbGoTop() ) } )
   Probe( "SetRelKey .5", {|| AdsSetRelKeyPos( 0.5 ) } )
   Probe( "SetRelKey 1",  {|| AdsSetRelKeyPos( 1.0 ) } )
   Probe( "SetRelKey 0",  {|| AdsSetRelKeyPos( 0.0 ) } )
   Probe( "SetRelKey .99",{|| AdsSetRelKeyPos( 0.99 ) } )
   Probe( "after RK Skip(+1)", {|| T->( DbSkip( 1 ) ) } )

   // --- Part B: the real TBrowse path --------------------------------
   ?
   ? "--- TBrowse ForceStable() ---"
   ProbeBrowse()

   ?
   ? "==> DESYNC HITS: " + Str( s_nDesync )
   ?

   CLOSE ALL
   RETURN

//----------------------------------------------------------------------

// Runs bOp, prints the rddads-side state, then probes FieldGet.
STATIC PROCEDURE Probe( cLabel, bOp )

   LOCAL lE, lB, nR, cRes := "ok", oErr

   Eval( bOp )

   lE := T->( Eof() )
   lB := T->( Bof() )
   nR := T->( RecNo() )

   BEGIN SEQUENCE WITH {| e | Break( e ) }
      T->( FieldGet( 1 ) )    // field 1 = NAME (character)
      T->( FieldGet( 3 ) )    // field 3 = AGE  (numeric)
   RECOVER USING oErr
      cRes := oErr:subsystem + "/" + LTrim( Str( oErr:subCode ) )
   END SEQUENCE

   IF cRes != "ok"
      cRes += "   <<<<< FieldGet RAISED"
      s_nDesync++
   ENDIF

   ? PadR( cLabel, 22 ), ;
     "Eof=" + iif( lE, "T", "F" ), ;
     "Bof=" + iif( lB, "T", "F" ), ;
     "Rec=" + Str( nR, 4 ), " -> ", cRes
   RETURN

// Drives a real TBrowse stabilize — the exact FORCESTABLE -> STABILIZE
// -> READRECORD path from Pritpal's stack trace.
STATIC PROCEDURE ProbeBrowse()

   LOCAL oTb, oErr, cRes := "ForceStable ok"

   BEGIN SEQUENCE WITH {| e | Break( e ) }
      oTb := TBrowseDB( 1, 1, 18, 78 )
      oTb:AddColumn( TBColumnNew( "NAME", {|| T->NAME } ) )
      oTb:AddColumn( TBColumnNew( "CITY", {|| T->CITY } ) )
      oTb:AddColumn( TBColumnNew( "AGE",  {|| T->AGE  } ) )
      T->( DbGoTop() )
      oTb:ForceStable()
   RECOVER USING oErr
      cRes := "ForceStable RAISED " + oErr:subsystem + "/" + ;
              LTrim( Str( oErr:subCode ) ) + " " + oErr:description
      s_nDesync++
   END SEQUENCE
   ? cRes
   RETURN

//----------------------------------------------------------------------

STATIC FUNCTION TempFolder()
   LOCAL c := GetEnv( "TEMP" )
   IF Empty( c ) ; c := GetEnv( "TMP" ) ; ENDIF
   IF Empty( c ) ; c := "C:\Temp" ; ENDIF
   RETURN c

#define PROBE_RECS  300

STATIC FUNCTION StageDbf( cDir, cDbf )
   LOCAL i, n
   IF ! hb_DirExists( cDir ) ; hb_DirCreate( cDir ) ; ENDIF
   IF File( cDbf ) ; FErase( cDbf ) ; ENDIF
   IF File( hb_FNameExtSet( cDbf, ".cdx" ) )
      FErase( hb_FNameExtSet( cDbf, ".cdx" ) )
   ENDIF
   DbCreate( cDbf, { { "NAME", "C", 20, 0 }, ;
                     { "CITY", "C", 20, 0 }, ;
                     { "AGE",  "N",  3, 0 } } )
   USE ( cDbf ) ALIAS _S SHARED NEW
   FOR i := 1 TO PROBE_RECS
      // pseudo-random NAME so the index order != record order
      n := ( i * 7919 ) % PROBE_RECS
      _S->( DbAppend() )
      _S->NAME := "Name" + StrZero( n, 5 )
      _S->CITY := "City" + StrZero( i % 50, 3 )
      _S->AGE  := 18 + ( i % 60 )
   NEXT
   _S->( DbCommit() )
   _S->( DbCloseArea() )
   RETURN nil
