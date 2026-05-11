// AdsSmoke.prg — headless smoke test driving OpenADS' ACE DLL through
// X#'s Advantage RDD (AXDBFCDX). The RDD P/Invokes ace32.dll /
// ace64.dll, so this exercises OpenADS' entry points end-to-end the
// way a real X# application would.
//
// Build with the X# compiler; put OpenADS' ace64.dll / ace32.dll first
// on PATH (NOT a SAP-shipped one). Exit code 0 = pass, non-zero = the
// first failed check.
//
// Uses only core RDD verbs (DbCreate / OrdCreate / DbAppend / DbSkip /
// DbSeek / DbDelete / DbRecall / FieldGet / FieldPut / ...) with string
// index expressions — no codeblocks, no _FIELD-> macro — so it builds
// across X# dialects.

#using System
#using System.IO

FUNCTION Start() AS INT
    TRY
        RETURN RunSmoke()
    CATCH e AS Exception
        ? "FAIL: exception " + e:GetType():FullName + ": " + e:Message
        RETURN 2
    END TRY

STATIC FUNCTION Fail(cMsg AS STRING) AS INT
    ? "FAIL:", cMsg
    RETURN 1

STATIC FUNCTION F1() AS STRING
    RETURN AllTrim(FieldGet(1))
STATIC FUNCTION F2() AS STRING
    RETURN AllTrim(FieldGet(2))
STATIC FUNCTION F4() AS STRING
    RETURN AllTrim(FieldGet(4))

STATIC FUNCTION RunSmoke() AS INT
    LOCAL cDir   AS STRING
    LOCAL cTable AS STRING

    ? "ace64.dll in use:", System.IntPtr.Size, "byte handles"
    cDir := Path.Combine(Path.GetTempPath(), "openads_xs_smoke")
    IF Directory.Exists(cDir)
        Directory.Delete(cDir, TRUE)
    ENDIF
    Directory.CreateDirectory(cDir)
    cTable := Path.Combine(cDir, "people")

    RddSetDefault("AXDBFCDX")

    LOCAL aStruct AS ARRAY
    aStruct := {                       ;
        { "NAME", "C", 20, 0 },        ;
        { "CITY", "C", 20, 0 },        ;
        { "AGE" , "N",  3, 0 },        ;
        { "NOTE", "M", 10, 0 }         ;
    }
    DbCreate(cTable, aStruct)

    IF .NOT. DbUseArea(TRUE, "AXDBFCDX", cTable, "people", FALSE)
        RETURN Fail("DbUseArea")
    ENDIF

    OrdCreate(cTable + ".cdx", "NAME", "NAME")
    OrdCreate(cTable + ".cdx", "CITY", "CITY")

    LOCAL aData AS ARRAY
    aData := {                                          ;
        { "Alice"  , "Madrid"   , 30, "first note"  },  ;
        { "Bob"    , "Barcelona", 41, "second note" },  ;
        { "Charlie", "Valencia" , 25, ""            },  ;
        { "Diana"  , "Sevilla"  , 37, "fourth"      }   ;
    }
    LOCAL i AS INT
    FOR i := 1 TO ALen(aData)
        DbAppend()
        FieldPut(1, aData[i, 1])
        FieldPut(2, aData[i, 2])
        FieldPut(3, aData[i, 3])
        FieldPut(4, aData[i, 4])
    NEXT
    DbCommit()

    IF LastRec() != 4
        RETURN Fail("LastRec() = " + AsString(LastRec()))
    ENDIF
    IF RecCount() != 4
        RETURN Fail("RecCount() = " + AsString(RecCount()))
    ENDIF

    // --- NAME-order navigation: GoTop / Skip +/- / GoBottom / Eof ---
    OrdSetFocus("NAME")
    DbGoTop()
    IF F1() != "Alice"
        RETURN Fail("NAME top = '" + F1() + "'")
    ENDIF
    DbSkip(1)
    IF F1() != "Bob"
        RETURN Fail("NAME skip+1 = '" + F1() + "'")
    ENDIF
    DbSkip(1)
    IF F1() != "Charlie"
        RETURN Fail("NAME skip+2 = '" + F1() + "'")
    ENDIF
    DbSkip(-1)
    IF F1() != "Bob"
        RETURN Fail("NAME skip-1 = '" + F1() + "'")
    ENDIF
    DbGoBottom()
    IF F1() != "Diana"
        RETURN Fail("NAME bottom = '" + F1() + "'")
    ENDIF
    DbSkip(1)
    IF .NOT. Eof()
        RETURN Fail("expected Eof() after bottom + skip+1")
    ENDIF

    // --- seek: hit, then a miss (hard seek) ---
    IF .NOT. DbSeek("Charlie") .OR. F2() != "Valencia"
        RETURN Fail("seek 'Charlie' -> Valencia")
    ENDIF
    DbSeek("Zzz")
    IF Found()
        RETURN Fail("seek 'Zzz' should not be Found()")
    ENDIF

    // --- memo (FPT) round-trip ---
    IF .NOT. DbSeek("Alice")
        RETURN Fail("re-seek 'Alice'")
    ENDIF
    IF F4() != "first note"
        RETURN Fail("memo Alice = '" + F4() + "'")
    ENDIF
    IF .NOT. DbSeek("Charlie")
        RETURN Fail("re-seek 'Charlie'")
    ENDIF
    IF F4() != ""
        RETURN Fail("memo Charlie should be empty, got '" + F4() + "'")
    ENDIF

    // --- delete / recall ---
    IF .NOT. DbSeek("Bob")
        RETURN Fail("re-seek 'Bob'")
    ENDIF
    DbDelete()
    IF .NOT. Deleted()
        RETURN Fail("Bob not Deleted() after DbDelete()")
    ENDIF
    DbRecall()
    IF Deleted()
        RETURN Fail("Bob still Deleted() after DbRecall()")
    ENDIF

    // --- replace a key field, re-read through the other index ---
    IF .NOT. DbSeek("Diana")
        RETURN Fail("re-seek 'Diana'")
    ENDIF
    FieldPut(2, "Bilbao")
    DbCommit()
    OrdSetFocus("CITY")
    DbGoTop()
    IF F2() != "Barcelona"
        RETURN Fail("CITY top = '" + F2() + "' (expected Barcelona)")
    ENDIF
    IF .NOT. DbSeek("Bilbao") .OR. F1() != "Diana"
        RETURN Fail("seek 'Bilbao' -> Diana (got '" + F1() + "')")
    ENDIF

    DbCloseArea()
    ? "OK: X# AXDBFCDX smoke passed (" + cDir + ")"
    RETURN 0
