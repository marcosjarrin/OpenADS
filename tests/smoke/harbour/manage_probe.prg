/*
 * Advantage Database Server RDD Management Functions Test program
 *
 * Copyright 2001 Brian Hays <bhays@abacuslaw.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * (License header trimmed — see manage.prg. This is a non-interactive
 *  variant of contrib/rddads/tests/manage.prg: the three WAIT pauses
 *  are removed and the server string is taken from the command line
 *  so the same telemetry can be captured locally and against a
 *  remote server.)
 */

#require "rddads"

#include "ord.ch"

REQUEST ADS

#if defined( __HBDYNLOAD__RDDADS__ )
#include "rddads.hbx"
#endif

PROCEDURE Main( cServer )

   LOCAL i
   LOCAL aRay

#if defined( __HBDYNLOAD__RDDADS__ )
   LOCAL l := hb_libLoad( hb_libName( "rddads" + hb_libPostfix() ) )

   hb_rddADSRegister()

   HB_SYMBOL_UNUSED( l )
#elif defined( __HBSCRIPT__HBSHELL )
   hb_rddADSRegister()
#endif

   IF Empty( cServer )
      cServer := "C:"
   ENDIF

   rddSetDefault( "ADS" )
   SET SERVER LOCAL    // REMOTE

   ? "Advantage Database Server Management Functions in Harbour"
   ?
   ? "Connect:", AdsMgConnect( cServer ), " server=" + cServer
   ?
   ? "AdsVersion( 0 ):", AdsVersion( 0 )
   ? "AdsVersion( 3 ):", AdsVersion( 3 )
   ?

   aRay := AdsMgGetInstallInfo()
   IF Len( aRay ) > 7
      ? "Install info:"
      ? aRay[ 1 ]
      ? aRay[ 2 ]
      ? aRay[ 3 ]
      ? aRay[ 4 ]
      ? aRay[ 5 ]
      ? aRay[ 6 ]
      ? aRay[ 7 ]
      ? aRay[ 8 ]
      ?
   ENDIF

   ? "Activity info:"
   ? AdsMgGetActivityInfo( 1 )
   ? AdsMgGetActivityInfo( 2 )

   aRay := AdsMgGetActivityInfo( 3 )
   IF Len( aRay ) > 3
      ? "Up Time:", aRay[ 1 ], aRay[ 2 ], aRay[ 3 ], aRay[ 4 ]
      ?
   ENDIF

   ?    "    Item          In Use     MaxUsed    Rejected"
   aRay := AdsMgGetActivityInfo( 4 )
   IF Len( aRay ) > 2
      ? "Users:         ", aRay[ 1 ], aRay[ 2 ], aRay[ 3 ]
   ENDIF

   aRay := AdsMgGetActivityInfo( 5 )
   IF Len( aRay ) > 2
      ? "Connections:   ", aRay[ 1 ], aRay[ 2 ], aRay[ 3 ]
   ENDIF

   aRay := AdsMgGetActivityInfo( 6 )
   IF Len( aRay ) > 2
      ? "WorkAreas:     ", aRay[ 1 ], aRay[ 2 ], aRay[ 3 ]
   ENDIF

   aRay := AdsMgGetActivityInfo( 7 )
   IF Len( aRay ) > 2
      ? "Tables:        ", aRay[ 1 ], aRay[ 2 ], aRay[ 3 ]
   ENDIF

   aRay := AdsMgGetActivityInfo( 8 )
   IF Len( aRay ) > 2
      ? "Indexes:       ", aRay[ 1 ], aRay[ 2 ], aRay[ 3 ]
   ENDIF

   aRay := AdsMgGetActivityInfo( 9 )
   IF Len( aRay ) > 2
      ? "Locks:         ", aRay[ 1 ], aRay[ 2 ], aRay[ 3 ]
   ENDIF

   aRay := AdsMgGetActivityInfo( 13 )
   IF Len( aRay ) > 2
      ? "WorkerThreads: ", aRay[ 1 ], aRay[ 2 ], aRay[ 3 ]
   ENDIF

   ?

   aRay := AdsMgGetCommStats()
   IF Len( aRay ) > 10
      ? aRay[  1 ], "% of pkts with checksum failures "
      ? aRay[  2 ], "Total packets received           "
      ? aRay[  3 ], "Receive packets out of sequence  "
      ? aRay[  4 ], "Packet owner not logged in       "
      ? aRay[  5 ], "Receive requests out of sequence "
      ? aRay[  6 ], "Checksum failures                "
      ? aRay[  7 ], "Server initiated disconnects     "
      ? aRay[  8 ], "Removed partial connections      "
   ENDIF

   ?

   aRay := AdsMgGetConfigInfo( 0 )
   IF Len( aRay ) > 24
      ? aRay[  1 ], " number connections            "
      ? aRay[  2 ], " number work areas             "
      ? aRay[  3 ], " number tables                 "
      ? aRay[  4 ], " number indexes                "
      ? aRay[  5 ], " number locks                  "
      ? aRay[ 15 ], " number worker threads         "
      ? aRay[ 24 ], " NT Service IP send port #     "
      ? aRay[ 25 ], " NT Service IP rcv port #      "
   ENDIF

   ?

   aRay := AdsMgGetConfigInfo( 1 )
   IF Len( aRay ) > 12
      ? aRay[  1 ], " Total mem taken by cfg params "
   ENDIF

   ?

   aRay := AdsMgGetUserNames()
   IF aRay != NIL
      ? "Number of connected users: ", Len( aRay )
      FOR i := 1 TO Len( aRay )
         ? aRay[ i ]
      NEXT
   ENDIF

   ?
   ? "Disconnect", AdsMgDisconnect()
   ?

   ? "end"
   ?

   RETURN
