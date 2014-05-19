#!/usr/bin/lua

local objex = require( "objex" )
local subex = require( "subex" )
local flgex = require( "flgex" )


print( _VERSION )
print( ("="):rep( 70 ) )
do
  print( "[ objex test ]" )
  local o = objex.new()
  o:print_me()
  print( "o.i =", o.i )
  o.i = 17
  print( "o.i =", o.i )
  o:print_me()
  print( ("="):rep( 70 ) )
end

do
  print( "[ subex test ]" )
  local p = subex.new()
  p:print_me()
  p.z = 3
  p.child.y = 2
  p.child.x = 1
  print( p.z, p.child, p.child.x, p.child.y )
  p:print_me()
  if _VERSION ~= "Lua 5.1" then
    for k,v in pairs( p.child ) do
      print( "p.child."..k, v )
    end
  end
  print( ("="):rep( 70 ) )
end

do
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

