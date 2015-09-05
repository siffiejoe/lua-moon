#!/usr/bin/lua

local objex = require( "objex" )
local flgex = require( "flgex" )


print( _VERSION )
do
  print( ("="):rep( 70 ) )
  print( "[ objex test ]" )
  local a = objex.newA()
  print( a, a.tag )
  a:printme()
  local b = a.b
  print( a.b, b, a.c, b.f )
  b:printme()
  a:switch()
  print( pcall( b.printme, b ) )
  print( a, a.tag )
  a:printme()
  local c = a.c
  print( a.c, c, a.b )
  c:printme()
  local d = c.d
  print( c.d, d, d.x, d.y )
  d:printme()
  d.x = 5
  d.y = 10
  print( d, d.x, d.y )
  d:printme()
  a:switch()
  print( pcall( d.printme, d ) )
  a:switch()
  d:printme()
  c:close()
  print( pcall( d.printme, d ) )
  local d2 = objex.getD()
  print( d2, d2.x, d2.y )
  d2:printme()
  local d3 = objex.makeD( 100, 200 )
  print( d3, d3.x, d3.y )
  d3:printme()
  local d4 = objex.newD()
  print( d4, d4.x, d4.y )
  d4:printme()
  local c2 = objex.newC()
  c2.d.x = 22
  c2.d.y = 44
  d2.x = 4
  d2.y = 8
  print( c2 )
  c2:printme()
  c2.d = d2
  c2:printme()
  c2:printmeD()
  d2:printme()
end
collectgarbage()


do
  print( ("="):rep( 70 ) )
  print( "[ flgex test ]" )
  local flags = flgex.NULL + flgex.ONE + flgex.TWO
  print( flags )
--[[
  print( "bitops work(1):", flgex.NULL | flgex.ONE | flgex.TWO == flags )
  print( "bitops work(2):", flags & ~flgex.ONE == flgex.TWO )
--]]
  print( "flags contains flgex.ONE?", flags( flgex.ONE ) )
  print( "flags contains flgex.TWO?", flags( flgex.TWO ) )
  flags = flags - flgex.ONE
  print( "flags contains flgex.ONE?", flags( flgex.ONE ) )
  print( "flags contains flgex.TWO?", flags( flgex.TWO ) )
  flags = flags - flgex.TWO
  print( "flags contains flgex.ONE?", flags( flgex.ONE ) )
  print( "flags contains flgex.TWO?", flags( flgex.TWO ) )
  print( "same and identical:", flags == flgex.NULL, flags, flgex.NULL )
  flags = (flgex.THREE + flgex.FOUR) - flgex.FOUR
  print( "same but not identical:", flags == flgex.THREE, flags, flgex.THREE )
  print( "better error message for mismatched types:" )
  print( pcall( function() local wrong = flgex.ONE + flgex.THREE end ) )
end

