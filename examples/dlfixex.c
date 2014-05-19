#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "plugin.h"

int main( void ) {
  int ret = EXIT_FAILURE;
  void* lib = dlopen( "./plugin.so", RTLD_LAZY|RTLD_LOCAL );
  void* sym = NULL;
  plugin f;
  if( !lib || !(sym = dlsym( lib, PLUGIN )) ) {
    fprintf( stderr, "ERROR: %s\n", dlerror() );
    goto error;
  }
  f = (plugin)sym;
  ret = f();
error:
  if( lib )
    dlclose( lib );
  return ret;
}

