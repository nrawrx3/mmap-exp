#if defined(MMAPEXT_API_BEING_BUILT)
#    if defined(_MSC_VER)
#        define MMAPEXT_API __declspec(dllexport)
#    else
#        define MMAPEXT_API __attribute__((visibility("default")))
#    endif
#elif defined(MMAPEXT_API_BEING_IMPORTED)
#    if defined(_MSC_VER)
#        define MMAPEXT_API __declspec(dllimport)
#    else
#        define MMAPEXT_API __attribute__((visibility("default")))
#    endif
#else
#define MMAPEXT_API
#endif
