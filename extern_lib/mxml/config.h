/* Local build configuration for vendored Mini-XML 4.0.5. */
#ifndef MXML_CONFIG_H
#define MXML_CONFIG_H

#define MXML_VERSION "4.0.5"

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#  define HAVE_PTHREAD_H 1
#endif

#endif
