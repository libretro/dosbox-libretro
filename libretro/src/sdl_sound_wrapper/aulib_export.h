
#ifndef AULIB_EXPORT_H
#define AULIB_EXPORT_H

#ifdef AULIB_STATIC_DEFINE
#  define AULIB_EXPORT
#  define AULIB_NO_EXPORT
#else
#  ifndef AULIB_EXPORT
#    ifdef SDL_audiolib_EXPORTS
        /* We are building this library */
#      define AULIB_EXPORT 
#    else
        /* We are using this library */
#      define AULIB_EXPORT 
#    endif
#  endif

#  ifndef AULIB_NO_EXPORT
#    define AULIB_NO_EXPORT 
#  endif
#endif

#ifndef AULIB_DEPRECATED
#  define AULIB_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef AULIB_DEPRECATED_EXPORT
#  define AULIB_DEPRECATED_EXPORT AULIB_EXPORT AULIB_DEPRECATED
#endif

#ifndef AULIB_DEPRECATED_NO_EXPORT
#  define AULIB_DEPRECATED_NO_EXPORT AULIB_NO_EXPORT AULIB_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef AULIB_NO_DEPRECATED
#    define AULIB_NO_DEPRECATED
#  endif
#endif

#endif
